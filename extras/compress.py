#!/usr/bin/env python3

import minify_html
import gzip
import bs4 

html = None
with open('web/index.html', 'r') as file:
    html = file.read()

soup = bs4.BeautifulSoup(html, 'html.parser')

dt_css = soup.find('head')
link_style_tags = soup.find_all('link', attrs={'rel': 'stylesheet'})
for link_style_tag in link_style_tags:
    with open(f'web/{link_style_tag["href"]}', 'r') as css_file:
        style_tag = soup.new_tag('style')
        style_tag.append(css_file.read())
        dt_css.insert(5, style_tag)
    link_style_tag.decompose()

script_tags = soup.find_all('script')
script_tags = [script_tag for script_tag in script_tags if script_tag.has_attr('src')]

for script_tag in script_tags:
    with open(f'web/{script_tag["src"]}', 'r') as js_file:
        script_tag.append(js_file.read())
        script_tag.attrs.clear()

data = str(soup)
data = minify_html.minify(
    data, 
    minify_js=True, 
    minify_css=True, 
    remove_processing_instructions=True, 
    keep_input_type_text_attr=True,
    keep_html_and_head_opening_tags=True,
    keep_closing_tags=True
)

data = data.encode()
data = gzip.compress(data)

content = ""
length = len(data)

for i in range(length):
    content += f'0x{data[i]:02x}, '
    if (i+1) % 20 == 0 and i > 0:
        content += '\n'

with open('../index.h', 'w') as h_file:
    h_file.write(f'#define index_html_gz_len {length}\n')
    h_file.write('const unsigned char index_html_gz[] = {\n')
    h_file.write(f'{content}\n')
    h_file.write('};\n')
