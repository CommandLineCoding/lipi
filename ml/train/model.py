"""PageNet: a small network that finds the four corners of a page.

Input is an RGB image resized to 256 x 256 with values in 0..255. The graph
normalises it itself, so callers only resize and convert. Outputs:

    corners      (N, 4, 2)  x, y in 0..1 of the image, ordered top-left,
                            top-right, bottom-right, bottom-left
    confidence   (N, 4)     peak probability of each corner heat map
    orientation  (N, 4)     logits for the number of 90 degree clockwise
                            turns that make the flattened page upright

During training the heat maps and a page mask are returned as well.
"""
import torch
import torch.nn as nn
import torch.nn.functional as F

INPUT_SIZE = 256
HEAT_SIZE = 64  # stride 4
MEAN = (0.485, 0.456, 0.406)
STD = (0.229, 0.224, 0.225)


def conv_bn(cin, cout, k=3, stride=1, groups=1, act=True):
    layers = [nn.Conv2d(cin, cout, k, stride, k // 2, groups=groups, bias=False), nn.BatchNorm2d(cout)]
    if act:
        layers.append(nn.ReLU6(inplace=True))
    return nn.Sequential(*layers)


class Block(nn.Module):
    """Inverted residual: expand, depth-wise 3x3, project."""

    def __init__(self, cin, cout, stride=1, expand=4):
        super().__init__()
        mid = cin * expand
        self.use_res = stride == 1 and cin == cout
        self.body = nn.Sequential(
            conv_bn(cin, mid, 1),
            conv_bn(mid, mid, 3, stride, groups=mid),
            conv_bn(mid, cout, 1, act=False),
        )

    def forward(self, x):
        y = self.body(x)
        return x + y if self.use_res else y


class PageNet(nn.Module):
    def __init__(self, width=1.0):
        super().__init__()
        c = lambda n: max(8, int(n * width))  # noqa: E731
        self.register_buffer("mean", torch.tensor(MEAN).view(1, 3, 1, 1) * 255.0)
        self.register_buffer("std", torch.tensor(STD).view(1, 3, 1, 1) * 255.0)
        self.stem = conv_bn(3, c(16), 3, 2)                                            # /2
        self.s2 = nn.Sequential(Block(c(16), c(24), 2, 3), Block(c(24), c(24)))        # /4
        self.s3 = nn.Sequential(Block(c(24), c(40), 2), Block(c(40), c(40)), Block(c(40), c(40)))  # /8
        self.s4 = nn.Sequential(Block(c(40), c(80), 2), Block(c(80), c(80)), Block(c(80), c(96)), Block(c(96), c(96)))  # /16
        self.s5 = nn.Sequential(Block(c(96), c(160), 2), Block(c(160), c(160)))        # /32
        self.up5 = conv_bn(c(160), c(96), 1)
        self.fuse4 = nn.Sequential(conv_bn(c(96) * 2, c(96), 1), Block(c(96), c(96), 1, 2))
        self.up4 = conv_bn(c(96), c(40), 1)
        self.fuse3 = nn.Sequential(conv_bn(c(40) * 2, c(48), 1), Block(c(48), c(48), 1, 2))
        self.up3 = conv_bn(c(48), c(24), 1)
        self.fuse2 = nn.Sequential(conv_bn(c(24) * 2, c(48), 1), Block(c(48), c(48), 1, 2))
        self.head = nn.Conv2d(c(48), 5, 1)  # four corner heat maps and a page mask
        self.orient = nn.Sequential(nn.Dropout(0.1), nn.Linear(c(160), 4))
        xs = (torch.arange(HEAT_SIZE, dtype=torch.float32) + 0.5) / HEAT_SIZE
        self.register_buffer("grid_x", xs.view(1, 1, 1, HEAT_SIZE))
        self.register_buffer("grid_y", xs.view(1, 1, HEAT_SIZE, 1))

    def forward(self, x, train_outputs=False):
        x = (x - self.mean) / self.std
        x = self.stem(x)
        f2 = self.s2(x)
        f3 = self.s3(f2)
        f4 = self.s4(f3)
        f5 = self.s5(f4)
        y = self.fuse4(torch.cat([F.interpolate(self.up5(f5), size=f4.shape[2:], mode="bilinear", align_corners=False), f4], 1))
        y = self.fuse3(torch.cat([F.interpolate(self.up4(y), size=f3.shape[2:], mode="bilinear", align_corners=False), f3], 1))
        y = self.fuse2(torch.cat([F.interpolate(self.up3(y), size=f2.shape[2:], mode="bilinear", align_corners=False), f2], 1))
        out = self.head(y)
        heat, mask = out[:, :4], out[:, 4:5]
        n = heat.shape[0]
        prob = F.softmax(heat.reshape(n, 4, -1), dim=2).reshape(n, 4, HEAT_SIZE, HEAT_SIZE)
        cx = (prob * self.grid_x).sum(dim=(2, 3))
        cy = (prob * self.grid_y).sum(dim=(2, 3))
        corners = torch.stack([cx, cy], dim=2)
        confidence = prob.amax(dim=(2, 3))
        orientation = self.orient(f5.mean(dim=(2, 3)))
        if train_outputs:
            return corners, confidence, orientation, heat, mask
        return corners, confidence, orientation


def count_parameters(model):
    return sum(p.numel() for p in model.parameters())
