import sys
import os
from PIL import Image, ImageFont, ImageDraw

CHARS = "一上下中为么于什以你信储光关写出刷前功助发可号听回图在地址备好存家就居已帘帮度开强当录息您成我扇手打扫报接描播新无是智没浏温灯片理电的窗管系线络统绪编网置署能视览设译试语误请返连送部重锁错门闭问间音页题风首武汉北京天气预报市多云晴阴雨雪未知更新完毕正在秒音频等待回复失败检查扫描发现个保存器大小雷阵取定绿色获选局有雾霾"

def main():
    font_path = "/mnt/c/Windows/Fonts/simsun.ttc"
    if not os.path.exists(font_path):
        font_path = "/mnt/c/Windows/Fonts/simhei.ttf"
    
    if not os.path.exists(font_path):
        print("ERROR: Font file not found!", file=sys.stderr)
        sys.exit(1)
        
    # Get unique characters
    seen = set()
    uniq_chars = []
    for ch in CHARS:
        if ch not in seen:
            seen.add(ch)
            uniq_chars.append(ch)
            
    # Sort characters by unicode code point! This is extremely important!
    uniq_chars.sort(key=lambda c: ord(c))
    
    font = ImageFont.truetype(font_path, 16)
    
    h_lines = []
    h_lines.append("/* cn_font.h — Generated Font Glyphs for Chinese Characters */")
    h_lines.append("#ifndef CN_FONT_H")
    h_lines.append("#define CN_FONT_H")
    h_lines.append("")
    h_lines.append("#include <stdint.h>")
    h_lines.append("")
    h_lines.append(f"static const uint32_t cn_char_codes[{len(uniq_chars)}] = {{")
    
    # Write character codes
    code_chunk = []
    for ch in uniq_chars:
        code_chunk.append(f"0x{ord(ch):04X}")
    
    # Format with comments showing characters
    for i in range(0, len(code_chunk), 8):
        chunk = code_chunk[i:i+8]
        chars_chunk = "".join(uniq_chars[i:i+8])
        h_lines.append("    " + ", ".join(chunk) + f", /* {chars_chunk} */")
    h_lines.append("};")
    h_lines.append("")
    
    h_lines.append(f"static const uint16_t cn_font16[{len(uniq_chars)}][16] = {{")
    
    # Draw and extract each character
    for idx, ch in enumerate(uniq_chars):
        img = Image.new("1", (16, 16), 0)
        draw = ImageDraw.Draw(img)
        # Using SimSun at 16pt, y=-2 offset aligns perfectly.
        draw.text((0, -2), ch, font=font, fill=1)
        
        rows = []
        for y in range(16):
            row_val = 0
            for x in range(16):
                if img.getpixel((x, y)):
                    row_val |= (1 << (15 - x))
            rows.append(f"0x{row_val:04X}")
            
        row_str = ", ".join(rows)
        h_lines.append(f"    /* [{idx:3d}] {ch} (0x{ord(ch):04X}) */ {{{row_str}}},")
        
    h_lines.append("};")
    h_lines.append("")
    
    # index lookup helper
    h_lines.append("static int get_cn_char_index(uint32_t code) {")
    h_lines.append("    int low = 0;")
    h_lines.append("    int high = (int)(sizeof(cn_char_codes) / sizeof(cn_char_codes[0])) - 1;")
    h_lines.append("    while (low <= high) {")
    h_lines.append("        int mid = low + (high - low) / 2;")
    h_lines.append("        if (cn_char_codes[mid] == code) {")
    h_lines.append("            return mid;")
    h_lines.append("        } else if (cn_char_codes[mid] < code) {")
    h_lines.append("            low = mid + 1;")
    h_lines.append("        } else {")
    h_lines.append("            high = mid - 1;")
    h_lines.append("        }")
    h_lines.append("    }")
    h_lines.append("    return -1; // Not found")
    h_lines.append("}")
    h_lines.append("")
    h_lines.append("#endif /* CN_FONT_H */")
    h_lines.append("")
    
    with open("/home/cyj/workspace/ai_assistant/cn_font.h", "w", encoding="utf-8") as f:
        f.write("\n".join(h_lines))
        
    print(f"Successfully generated {len(uniq_chars)} characters into cn_font.h!")

if __name__ == "__main__":
    main()
