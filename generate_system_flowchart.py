# -*- coding: utf-8 -*-
from pathlib import Path
import math

from PIL import Image, ImageDraw, ImageFont


OUT = Path(r"F:\ESP\shoubiaob\第三章_系统总体软件流程图.png")


def load_font(size, bold=False):
    candidates = [
        r"C:\Windows\Fonts\msyhbd.ttc" if bold else r"C:\Windows\Fonts\msyh.ttc",
        r"C:\Windows\Fonts\simhei.ttf",
        r"C:\Windows\Fonts\simsun.ttc",
    ]
    for path in candidates:
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            pass
    return ImageFont.load_default()


W, H = 1800, 2260
BG = (248, 250, 252)
BOX = (255, 255, 255)
BOX_BLUE = (235, 245, 255)
BOX_GREEN = (236, 253, 245)
BOX_ORANGE = (255, 247, 237)
DECISION = (255, 250, 235)
ACCENT = (37, 99, 235)
TEXT = (15, 23, 42)
LINE = (71, 85, 105)
MUTED = (100, 116, 139)

img = Image.new("RGB", (W, H), BG)
draw = ImageDraw.Draw(img)

font_title = load_font(48, True)
font_box = load_font(31)
font_small = load_font(25)
font_note = load_font(24)


def text_size(text, font):
    box = draw.multiline_textbbox((0, 0), text, font=font, spacing=8)
    return box[2] - box[0], box[3] - box[1]


def center_text(cx, cy, text, font, fill=TEXT):
    tw, th = text_size(text, font)
    draw.multiline_text((cx - tw / 2, cy - th / 2), text, font=font, fill=fill, align="center", spacing=8)


def rounded_box(x, y, w, h, text, fill=BOX, outline=ACCENT, font=font_box):
    draw.rounded_rectangle((x, y, x + w, y + h), radius=24, fill=fill, outline=outline, width=4)
    center_text(x + w / 2, y + h / 2, text, font)
    return {"x": x, "y": y, "w": w, "h": h, "cx": x + w / 2, "cy": y + h / 2}


def diamond(cx, cy, w, h, text):
    pts = [(cx, cy - h / 2), (cx + w / 2, cy), (cx, cy + h / 2), (cx - w / 2, cy)]
    draw.polygon(pts, fill=DECISION, outline=ACCENT)
    draw.line(pts + [pts[0]], fill=ACCENT, width=4)
    center_text(cx, cy, text, font_small)
    return {"x": cx - w / 2, "y": cy - h / 2, "w": w, "h": h, "cx": cx, "cy": cy}


def arrowhead(x1, y1, x2, y2):
    angle = math.atan2(y2 - y1, x2 - x1)
    size = 17
    p1 = (x2 - size * math.cos(angle - math.pi / 6), y2 - size * math.sin(angle - math.pi / 6))
    p2 = (x2 - size * math.cos(angle + math.pi / 6), y2 - size * math.sin(angle + math.pi / 6))
    draw.polygon([(x2, y2), p1, p2], fill=LINE)


def label(x, y, text):
    tw, th = text_size(text, font_note)
    draw.rounded_rectangle((x - tw / 2 - 10, y - th / 2 - 6, x + tw / 2 + 10, y + th / 2 + 6),
                           radius=9, fill=BG)
    center_text(x, y, text, font_note, LINE)


def line_arrow(points, text=None, text_at=None):
    for (x1, y1), (x2, y2) in zip(points, points[1:]):
        draw.line((x1, y1, x2, y2), fill=LINE, width=4)
    arrowhead(*points[-2], *points[-1])
    if text:
        if text_at is None:
            text_at = ((points[0][0] + points[-1][0]) / 2, (points[0][1] + points[-1][1]) / 2)
        label(text_at[0], text_at[1], text)


def down(a, b, text=None):
    line_arrow([(a["cx"], a["y"] + a["h"]), (b["cx"], b["y"])], text=text)


title = "智能小电视系统总体软件流程图"
tw, _ = text_size(title, font_title)
draw.text(((W - tw) / 2, 50), title, font=font_title, fill=TEXT)

cx = W // 2
main_w = 560

start = rounded_box(cx - main_w / 2, 145, main_w, 105, "系统上电 / 复位", BOX_BLUE)
init = rounded_box(cx - main_w / 2, 310, main_w, 135, "硬件初始化\nGPIO、SPI、I2S、LCD、按键、LED")
wifi = rounded_box(cx - main_w / 2, 505, main_w, 120, "连接 WiFi\n初始化小智 AI 通信协议")
home = rounded_box(cx - main_w / 2, 690, main_w, 125, "进入主页待机\n显示时间、日期、天气、电量", BOX_BLUE)
wake = diamond(cx, 955, 470, 180, "是否检测到\n唤醒词？")
hold = rounded_box(185, 895, 360, 120, "未唤醒\n保持主页待机", BOX_BLUE, font=font_small)
chat = rounded_box(cx - main_w / 2, 1165, main_w, 120, "切换到聊天页\n打开音频通道")
record = rounded_box(cx - main_w / 2, 1345, main_w, 130, "采集麦克风语音\n上传至小智 AI 云端")
cloud = rounded_box(cx - main_w / 2, 1540, main_w, 130, "云端识别与理解\nASR / LLM / TTS")
kind = diamond(cx, 1785, 510, 180, "返回结果类型？")

qa1 = rounded_box(110, 1940, 420, 120, "普通问答\n扬声器播放 AI 回复")
qa2 = rounded_box(110, 2110, 420, 105, "5 秒后\n返回主页待机", BOX_BLUE)

timer1 = rounded_box(690, 1940, 420, 120, "倒计时指令\n进入倒计时页面", BOX_ORANGE)
timer2 = rounded_box(690, 2110, 420, 105, "倒计时结束\n响铃 5 秒", BOX_ORANGE)

led1 = rounded_box(1270, 1940, 420, 120, "LED 控制指令\nIO10 输出高 / 低电平", BOX_GREEN)
led2 = rounded_box(1270, 2110, 420, 105, "执行完成\n返回主页待机", BOX_BLUE)

down(start, init)
down(init, wifi)
down(wifi, home)
down(home, wake)
down(wake, chat, "是")
down(chat, record)
down(record, cloud)
down(cloud, kind)

line_arrow([(wake["x"], wake["cy"]), (hold["x"] + hold["w"], hold["cy"])], "否", (590, 920))

line_arrow([(kind["x"], kind["cy"]), (qa1["cx"], kind["cy"]), (qa1["cx"], qa1["y"])], "问答", (610, 1818))
down(qa1, qa2)

line_arrow([(kind["cx"], kind["y"] + kind["h"]), (timer1["cx"], timer1["y"])], "倒计时", (cx + 95, 1900))
down(timer1, timer2)

line_arrow([(kind["x"] + kind["w"], kind["cy"]), (led1["cx"], kind["cy"]), (led1["cx"], led1["y"])], "开灯 / 关灯", (1190, 1818))
down(led1, led2)

draw.rounded_rectangle((105, 1160, 465, 1295), radius=18, fill=(241, 245, 249), outline=(148, 163, 184), width=3)
center_text(285, 1227, "KEY1：取消倒计时 / 停止响铃\n长按返回主页", font_note, MUTED)

draw.rounded_rectangle((105, 1325, 465, 1410), radius=18, fill=(241, 245, 249), outline=(148, 163, 184), width=3)
center_text(285, 1367, "KEY2：音量调节", font_note, MUTED)

img.save(OUT)
print(str(OUT))
