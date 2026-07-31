#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
CH32V305 UAC2 - HID 校准上位机 (PyQt6 + hidapi)

通信协议（与固件一致）：
  - 读取校准：EP0 控制端点 GET_REPORT，一次取回整个 struct UacParam（40 字节）
  - 写入校准：HID OUT 端点（40 字节 Output 报告），两步命令：
      cmd=0 (UPDATE)：命令字 + 9 个反馈值 → 更新设备内存缓存
      cmd=1 (SAVE)  ：把内存缓存写入 flash

struct UacParam 内存布局（小端，40 字节）：
    magic                 uint32  0x53484344 ("DCHS")
    sr48k.ceil/normal/floor  3x uint32
    sr96k.ceil/normal/floor  3x uint32
    sr192k.ceil/normal/floor 3x uint32

HID OUT 报告（40 字节）：
    cmd(uint32) + sr48k(ceil,normal,floor) + sr96k(...) + sr192k(...)   // UPDATE
    cmd(uint32) + 填充 0                                                  // SAVE

依赖：pip install PyQt6 hid
"""

import struct
import sys

import hid
from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import (
    QApplication, QWidget, QLabel, QSpinBox, QPushButton, QComboBox,
    QGridLayout, QHBoxLayout, QVBoxLayout, QGroupBox,
)

# ---------------------------------------------------------------------------
# 协议常量（与固件 uac_param.h 一致）
# ---------------------------------------------------------------------------
VID = 0x1A86
PID = 0x0001
USAGE_PAGE = 0xFF00  # Vendor Defined

UAC_PARAM_MAGIC = 0x53484344
UAC_PARAM_FMT = "<I 3I 3I 3I"          # 40 字节（含 magic）
UAC_PARAM_SIZE = struct.calcsize(UAC_PARAM_FMT)   # 40

# HID OUT 报告：40 字节 Output 报告（与报告描述符 Report Count 40 一致）
HID_REPORT_SIZE = 40

# HID 命令（与 main.c 的 enum HidCommand 一致）
HID_CMD_UPDATE = 0
HID_CMD_SAVE = 1

# UPDATE 报告：cmd(4B) + 9 x uint32(36B) = 40B
HID_UPDATE_FMT = "<I 9I"

SR_NAMES = ["sr48k", "sr96k", "sr192k"]
FIELD_NAMES = ["ceil", "normal", "floor"]

# 各采样率档位合法范围（标称 ±0.5%，与固件 UacParam_Clamp 一致）
SR_RANGES = {
    "sr48k": (47760, 48240),
    "sr96k": (95520, 96480),
    "sr192k": (191040, 192960),
}


# ---------------------------------------------------------------------------
# 编解码
# ---------------------------------------------------------------------------
def unpack_param(data):
    """解析 UacParam（不含报告 ID），list/bytes 均可；校验 magic"""
    data = bytes(data[:UAC_PARAM_SIZE])
    if len(data) < UAC_PARAM_SIZE:
        raise ValueError(f"数据过短: {len(data)}B < {UAC_PARAM_SIZE}B")
    magic, *rest = struct.unpack(UAC_PARAM_FMT, data)
    if magic != UAC_PARAM_MAGIC:
        raise ValueError(f"magic 不匹配: 0x{magic:08X} != 0x{UAC_PARAM_MAGIC:08X}")
    return [list(rest[i * 3:(i + 1) * 3]) for i in range(3)]


def build_update(srs):
    """UPDATE 命令报告（40 字节）：cmd=0 + 9 个反馈值"""
    flat = [v for sr in srs for v in sr]
    return struct.pack(HID_UPDATE_FMT, HID_CMD_UPDATE, *flat)


def build_save():
    """SAVE 命令报告（40 字节）：cmd=1 + 填充 0"""
    return struct.pack(HID_UPDATE_FMT, HID_CMD_SAVE, *([0] * 9))


# ---------------------------------------------------------------------------
# hidapi 封装
# ---------------------------------------------------------------------------
def list_devices():
    """枚举 VID/PID 匹配的 HID 设备"""
    return [d for d in hid.enumerate(VID, PID)]


def open_device(path=None):
    dev = hid.device()
    if path:
        dev.open_path(path)
    else:
        dev.open(VID, PID)
    return dev


def read_param(dev):
    """EP0 GET_REPORT 读取整个 UacParam。返回 [[ceil,normal,floor] x3]"""
    # max_length 必须 >= 41（40 数据 + 1 报告 ID，Windows FeatureReportByteLength=41）
    buf = dev.get_feature_report(0, 64)
    # buf[0] 是报告 ID，实际数据从 buf[1] 开始
    return unpack_param(buf[1:])


def write_report(dev, report):
    """HID OUT 端点发送 40 字节报告。返回发送字节数"""
    packet = b"\x00" + report              # 报告 ID 0 + 40B 报告
    return dev.write(packet)


# ---------------------------------------------------------------------------
# GUI
# ---------------------------------------------------------------------------
class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("UAC2 校准 - HID 参数读写 (CH32V305)")
        self.setMinimumWidth(520)
        self.dev = None
        self.edits = {}
        self._build_ui()
        self.refresh_devices()

    def _build_ui(self):
        root = QVBoxLayout(self)

        # ---- 设备选择 ----
        dev_row = QHBoxLayout()
        dev_row.addWidget(QLabel("设备:"))
        self.dev_combo = QComboBox()
        self.dev_combo.setMinimumWidth(320)
        dev_row.addWidget(self.dev_combo, 1)
        btn_refresh = QPushButton("刷新")
        btn_refresh.clicked.connect(self.refresh_devices)
        dev_row.addWidget(btn_refresh)
        btn_open = QPushButton("打开")
        btn_open.clicked.connect(self.open_selected)
        dev_row.addWidget(btn_open)
        root.addLayout(dev_row)

        # ---- 参数编辑 ----
        grid = QGridLayout()
        grid.addWidget(QLabel(""), 0, 0)
        for c, name in enumerate(FIELD_NAMES, start=1):
            grid.addWidget(QLabel(name), 0, c)
        for r, sr in enumerate(SR_NAMES, start=1):
            grid.addWidget(QLabel(sr), r, 0)
            lo, hi = SR_RANGES[sr]
            for c, f in enumerate(FIELD_NAMES, start=1):
                sb = QSpinBox()
                sb.setRange(lo, hi)          # UI 直接限制合法范围
                sb.setValue(lo)
                sb.setMinimumWidth(100)
                self.edits[(sr, f)] = sb
                grid.addWidget(sb, r, c)
        box = QGroupBox("UacParam（各采样率反馈采样率 ceil/normal/floor）")
        box.setLayout(grid)
        root.addWidget(box)

        # ---- 操作按钮 ----
        btn_row = QHBoxLayout()
        btn_read = QPushButton("读取校准")
        btn_read.setToolTip("EP0 GET_REPORT，一次取回整个 UacParam")
        btn_read.clicked.connect(self.do_read)
        btn_row.addWidget(btn_read)
        btn_update = QPushButton("更新缓存")
        btn_update.setToolTip("HID OUT 命令0：只更新设备内存，不写 flash")
        btn_update.clicked.connect(self.do_update)
        btn_row.addWidget(btn_update)
        btn_save = QPushButton("保存到 Flash")
        btn_save.setToolTip("HID OUT 命令1：把内存缓存写入 flash")
        btn_save.clicked.connect(self.do_save)
        btn_row.addWidget(btn_save)
        root.addLayout(btn_row)

        self.status = QLabel("未打开设备")
        self.status.setWordWrap(True)
        root.addWidget(self.status)

    # ---- 设备管理 ----
    def refresh_devices(self):
        self.dev_combo.clear()
        try:
            devices = list_devices()
        except Exception as e:
            self.status.setText(f"枚举失败: {e}")
            return
        for d in devices:
            label = f"{d.get('product_string', 'HID')}  VID={d['vendor_id']:04X} PID={d['product_id']:04X}"
            self.dev_combo.addItem(label, d.get("path"))
        self.status.setText(f"找到 {len(devices)} 个设备（VID={VID:04X} PID={PID:04X}）")

    def open_selected(self):
        if self.dev is not None:
            try:
                self.dev.close()
            except Exception:
                pass
            self.dev = None
        path = self.dev_combo.currentData()
        try:
            self.dev = open_device(path)
        except Exception as e:
            self.status.setText(f"打开失败: {e}")
            return
        self.status.setText("设备已打开")

    # ---- 读取 ----
    def do_read(self):
        if self.dev is None:
            self.status.setText("请先打开设备")
            return
        try:
            srs = read_param(self.dev)
        except Exception as e:
            self.status.setText(f"读取失败: {e}")
            return
        for i, sr in enumerate(SR_NAMES):
            for j, f in enumerate(FIELD_NAMES):
                self.edits[(sr, f)].setValue(srs[i][j])
        self.status.setText("读取成功（EP0 GET_REPORT，整个 UacParam）")

    def _collect_values(self):
        # QSpinBox 已限制范围，直接取值
        return [[self.edits[(sr, f)].value() for f in FIELD_NAMES] for sr in SR_NAMES]

    # ---- 更新缓存（HID OUT 命令0）----
    def do_update(self):
        if self.dev is None:
            self.status.setText("请先打开设备")
            return
        try:
            srs = self._collect_values()
            n = write_report(self.dev, build_update(srs))
        except Exception as e:
            self.status.setText(f"更新失败: {e}")
            return
        self.status.setText(f"已更新内存缓存（HID OUT UPDATE，{n} 字节）")

    # ---- 保存到 Flash（HID OUT 命令1）----
    def do_save(self):
        if self.dev is None:
            self.status.setText("请先打开设备")
            return
        try:
            n = write_report(self.dev, build_save())
        except Exception as e:
            self.status.setText(f"保存失败: {e}")
            return
        self.status.setText(f"已写入 flash（HID OUT SAVE，{n} 字节）")


def main():
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
