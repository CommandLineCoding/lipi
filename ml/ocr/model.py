"""The recogniser: a small convolutional network over the line, a bidirectional LSTM along it, and CTC on the output.

Input is a grayscale line 48 pixels high and any width; the output has one step per 4 pixels of width. About 0.8
million parameters, so it stays small on a phone. Only layers that ncnn supports are used.
"""
import torch
import torch.nn as nn

LINE_HEIGHT = 48
STRIDE = 4  # input pixels per output step


def conv_bn(i, o, k, s, groups=1):
    return nn.Sequential(nn.Conv2d(i, o, k, s, k // 2, groups=groups, bias=False), nn.BatchNorm2d(o), nn.ReLU(inplace=True))


def ds_block(i, o, s):
    """Depth-wise 3x3 then point-wise 1x1."""
    return nn.Sequential(conv_bn(i, i, 3, s, groups=i), conv_bn(i, o, 1, 1))


class RecNet(nn.Module):
    def __init__(self, num_classes, hidden=128):
        super().__init__()
        self.features = nn.Sequential(
            conv_bn(1, 24, 3, (2, 2)),        # 24 x W/2
            ds_block(24, 48, (2, 2)),         # 12 x W/4
            ds_block(48, 96, (2, 1)),         # 6 x W/4
            ds_block(96, 96, 1),
            ds_block(96, 160, (2, 1)),        # 3 x W/4
            ds_block(160, 160, 1),
        )
        self.pool = nn.AdaptiveAvgPool2d((1, None))
        self.rnn = nn.LSTM(160, hidden, num_layers=2, bidirectional=True, batch_first=True)
        self.head = nn.Linear(hidden * 2, num_classes)

    def forward(self, x):
        """x: (N, 1, 48, W) in 0..1. Returns log-probabilities of shape (T, N, classes), the layout CTC loss wants."""
        f = self.pool(self.features(x)).squeeze(2).transpose(1, 2)  # N, T, C
        out, _ = self.rnn(f)
        return self.head(out).transpose(0, 1).log_softmax(2)


def count_parameters(model):
    return sum(p.numel() for p in model.parameters())
