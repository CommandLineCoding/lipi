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
    def __init__(self, num_classes, hidden=128, width=1.0):
        super().__init__()
        c = lambda n: max(8, int(round(n * width / 8)) * 8)
        self.features = nn.Sequential(
            conv_bn(1, c(24), 3, (2, 2)),             # 24 x W/2
            ds_block(c(24), c(48), (2, 2)),           # 12 x W/4
            ds_block(c(48), c(96), (2, 1)),           # 6 x W/4
            ds_block(c(96), c(96), 1),
            ds_block(c(96), c(160), (2, 1)),          # 3 x W/4
            ds_block(c(160), c(160), 1),
        )
        self.rnn = nn.LSTM(c(160), hidden, num_layers=2, bidirectional=True, batch_first=True)
        self.head = nn.Linear(hidden * 2, num_classes)

    def forward(self, x, steps=None):
        """x: (N, 1, 48, W) in 0..1. Returns log-probabilities of shape (T, N, classes), the layout CTC loss wants.

        When the images in a batch were padded to a common width, pass `steps`, the number of real output steps of each
        image, so that the recurrent layer ignores the padding. Without this the network learns to depend on padding that
        a single line, read on its own, does not have (a word read alone scored about 5 times worse than the same word
        in a padded batch)."""
        f = self.features(x).mean(2).transpose(1, 2)  # average the remaining rows: N, T, C
        if steps is None:
            out, _ = self.rnn(f)
        else:
            packed = nn.utils.rnn.pack_padded_sequence(f, steps.cpu(), batch_first=True, enforce_sorted=False)
            out, _ = self.rnn(packed)
            out, _ = nn.utils.rnn.pad_packed_sequence(out, batch_first=True, total_length=f.shape[1])
        return self.head(out).transpose(0, 1).log_softmax(2)


def count_parameters(model):
    return sum(p.numel() for p in model.parameters())
