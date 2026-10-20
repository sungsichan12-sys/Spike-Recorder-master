"""Bundle only public website sources into one DriveX-ready index.html."""
from pathlib import Path
import base64
import re

ROOT=Path(__file__).resolve().parent.parent
SCRIPTS={"arena-config.js","arena-client.js","arena-cloud-client.js","arena-page.js"}
ASSETS={"assets/school-mark.png","assets/lottol-mark.png"}


def bundle():
    html=(ROOT/"index.html").read_text(encoding="utf-8")
    found=set()
    def inline(match):
        name=match[1]
        if name not in SCRIPTS:
            raise ValueError("Unexpected public script: "+name)
        source=(ROOT/name).read_text(encoding="utf-8")
        if name=="arena-config.js":
            source='/*LOTTOL_CLOUD_CONFIG*/'+source.strip()+'/*LOTTOL_CLOUD_CONFIG_END*/'
        found.add(name)
        return "<script>\n"+source.replace("</script","<\\/script")+"\n</script>"
    html=re.sub(r'<script src="\./([a-z-]+\.js)"></script>',inline,html)
    if found!=SCRIPTS:
        raise ValueError("Not all website sources were bundled")
    def image(match):
        name=match[2]
        if name not in ASSETS:
            raise ValueError("Unexpected public image: "+name)
        encoded=base64.b64encode((ROOT/name).read_bytes()).decode("ascii")
        return match[1]+'="data:image/png;base64,'+encoded+'"'
    html=re.sub(r'(src|href)="\./(assets/[a-z-]+\.png)"',image,html)
    output=ROOT/"drivex-site/index.html"
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(html,encoding="utf-8")
    return output


if __name__=="__main__":
    print(bundle())
