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
# i18n（中 / 英）
# ---------------------------------------------------------------------------
TRANSLATIONS = {
    "zh": {
        "window_title": "UAC2 校准 - HID 参数读写 (CH32V305)",
        "lang": "语言:",
        "device": "设备:",
        "refresh": "刷新",
        "open": "打开",
        "param_group": "UacParam（各采样率反馈采样率 ceil/normal/floor）",
        "read": "读取校准",
        "read_tip": "EP0 GET_REPORT，一次取回整个 UacParam",
        "update": "更新缓存",
        "update_tip": "HID OUT 命令0：只更新设备内存，不写 flash",
        "save": "保存到 Flash",
        "save_tip": "HID OUT 命令1：把内存缓存写入 flash",
        "not_open": "未打开设备",
        "open_ok": "设备已打开",
        "open_fail": "打开失败: {e}",
        "enum_fail": "枚举失败: {e}",
        "found_n": "找到 {n} 个设备（VID={vid:04X} PID={pid:04X}）",
        "please_open": "请先打开设备",
        "read_ok": "读取成功（EP0 GET_REPORT，整个 UacParam）",
        "read_fail": "读取失败: {e}",
        "update_ok": "已更新内存缓存（HID OUT UPDATE，{n} 字节）",
        "update_fail": "更新失败: {e}",
        "save_ok": "已写入 flash（HID OUT SAVE，{n} 字节）",
        "save_fail": "保存失败: {e}",
    },
    "en": {
        "window_title": "UAC2 Calibration - HID Param R/W (CH32V305)",
        "lang": "Language:",
        "device": "Device:",
        "refresh": "Refresh",
        "open": "Open",
        "param_group": "UacParam (feedback sample rates ceil/normal/floor)",
        "read": "Read Calibration",
        "read_tip": "EP0 GET_REPORT, read whole UacParam at once",
        "update": "Update Cache",
        "update_tip": "HID OUT cmd 0: update device RAM cache only, no flash",
        "save": "Save to Flash",
        "save_tip": "HID OUT cmd 1: write cache to flash",
        "not_open": "Device not opened",
        "open_ok": "Device opened",
        "open_fail": "Open failed: {e}",
        "enum_fail": "Enumerate failed: {e}",
        "found_n": "Found {n} device(s) (VID={vid:04X} PID={pid:04X})",
        "please_open": "Please open a device first",
        "read_ok": "Read OK (EP0 GET_REPORT, whole UacParam)",
        "read_fail": "Read failed: {e}",
        "update_ok": "Cache updated (HID OUT UPDATE, {n} bytes)",
        "update_fail": "Update failed: {e}",
        "save_ok": "Written to flash (HID OUT SAVE, {n} bytes)",
        "save_fail": "Save failed: {e}",
    },
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
        self.lang = "zh"
        self.setMinimumWidth(520)
        self.dev = None
        self.edits = {}
        self.devices = []
        self.container = None
        # 固定顶层布局，语言切换时替换内容容器
        self._outer = QVBoxLayout(self)
        self._build_ui()
        self.refresh_devices()

    def _tr(self, key, **kw):
        text = TRANSLATIONS.get(self.lang, TRANSLATIONS["zh"]).get(key, key)
        return text.format(**kw) if kw else text

    def _rebuild(self):
        # 保留编辑值与设备选中，重建 UI（语言切换后刷新文本）
        values = None
        if self.edits:
            values = self._collect_values()
        path = self.dev_combo.currentData() if getattr(self, "dev_combo", None) else None
        self.edits = {}
        self._build_ui()
        for d in self.devices:
            label = f"{d.get('product_string', 'HID')}  VID={d['vendor_id']:04X} PID={d['product_id']:04X}"
            self.dev_combo.addItem(label, d.get("path"))
        if path is not None:
            idx = self.dev_combo.findData(path)
            if idx >= 0:
                self.dev_combo.setCurrentIndex(idx)
        if values:
            for i, sr in enumerate(SR_NAMES):
                for j, f in enumerate(FIELD_NAMES):
                    self.edits[(sr, f)].setValue(values[i][j])

    def on_lang_changed(self, index):
        self.lang = "zh" if index == 0 else "en"
        self._rebuild()

    def _build_ui(self):
        self.setWindowTitle(self._tr("window_title"))
        # 替换内容容器（避免 QWidget 重复 setLayout 警告）
        if self.container is not None:
            self._outer.removeWidget(self.container)
            self.container.deleteLater()
        self.container = QWidget(self)
        self._outer.addWidget(self.container)
        root = QVBoxLayout(self.container)

        # ---- 语言选择 ----
        lang_row = QHBoxLayout()
        lang_row.addWidget(QLabel(self._tr("lang")))
        self.lang_combo = QComboBox()
        self.lang_combo.addItems(["中文", "English"])
        self.lang_combo.setCurrentIndex(0 if self.lang == "zh" else 1)
        self.lang_combo.currentIndexChanged.connect(self.on_lang_changed)
        lang_row.addWidget(self.lang_combo)
        lang_row.addStretch(1)
        root.addLayout(lang_row)

        # ---- 设备选择 ----
        dev_row = QHBoxLayout()
        dev_row.addWidget(QLabel(self._tr("device")))
        self.dev_combo = QComboBox()
        self.dev_combo.setMinimumWidth(320)
        dev_row.addWidget(self.dev_combo, 1)
        btn_refresh = QPushButton(self._tr("refresh"))
        btn_refresh.clicked.connect(self.refresh_devices)
        dev_row.addWidget(btn_refresh)
        btn_open = QPushButton(self._tr("open"))
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
        box = QGroupBox(self._tr("param_group"))
        box.setLayout(grid)
        root.addWidget(box)

        # ---- 操作按钮 ----
        btn_row = QHBoxLayout()
        btn_read = QPushButton(self._tr("read"))
        btn_read.setToolTip(self._tr("read_tip"))
        btn_read.clicked.connect(self.do_read)
        btn_row.addWidget(btn_read)
        btn_update = QPushButton(self._tr("update"))
        btn_update.setToolTip(self._tr("update_tip"))
        btn_update.clicked.connect(self.do_update)
        btn_row.addWidget(btn_update)
        btn_save = QPushButton(self._tr("save"))
        btn_save.setToolTip(self._tr("save_tip"))
        btn_save.clicked.connect(self.do_save)
        btn_row.addWidget(btn_save)
        root.addLayout(btn_row)

        self.status = QLabel(self._tr("not_open"))
        self.status.setWordWrap(True)
        root.addWidget(self.status)

    # ---- 设备管理 ----
    def refresh_devices(self):
        self.dev_combo.clear()
        try:
            self.devices = list_devices()
        except Exception as e:
            self.status.setText(self._tr("enum_fail", e=e))
            return
        for d in self.devices:
            label = f"{d.get('product_string', 'HID')}  VID={d['vendor_id']:04X} PID={d['product_id']:04X}"
            self.dev_combo.addItem(label, d.get("path"))
        self.status.setText(self._tr("found_n", n=len(self.devices), vid=VID, pid=PID))

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
            self.status.setText(self._tr("open_fail", e=e))
            return
        self.status.setText(self._tr("open_ok"))

    # ---- 读取 ----
    def do_read(self):
        if self.dev is None:
            self.status.setText(self._tr("please_open"))
            return
        try:
            srs = read_param(self.dev)
        except Exception as e:
            self.status.setText(self._tr("read_fail", e=e))
            return
        for i, sr in enumerate(SR_NAMES):
            for j, f in enumerate(FIELD_NAMES):
                self.edits[(sr, f)].setValue(srs[i][j])
        self.status.setText(self._tr("read_ok"))

    def _collect_values(self):
        # QSpinBox 已限制范围，直接取值
        return [[self.edits[(sr, f)].value() for f in FIELD_NAMES] for sr in SR_NAMES]

    # ---- 更新缓存（HID OUT 命令0）----
    def do_update(self):
        if self.dev is None:
            self.status.setText(self._tr("please_open"))
            return
        try:
            srs = self._collect_values()
            n = write_report(self.dev, build_update(srs))
        except Exception as e:
            self.status.setText(self._tr("update_fail", e=e))
            return
        self.status.setText(self._tr("update_ok", n=n))

    # ---- 保存到 Flash（HID OUT 命令1）----
    def do_save(self):
        if self.dev is None:
            self.status.setText(self._tr("please_open"))
            return
        try:
            n = write_report(self.dev, build_save())
        except Exception as e:
            self.status.setText(self._tr("save_fail", e=e))
            return
        self.status.setText(self._tr("save_ok", n=n))


def main():
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
