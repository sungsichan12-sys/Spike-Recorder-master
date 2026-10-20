# LOTTOL · DriveX 클라우드 랭킹

공개 홈페이지 폴더: https://drive.google.com/drive/folders/1LwVyq5kHoEvFfaSmj--GKY7fMGwckk8u

홈페이지는 DriveX에서 제공하고, 점수 저장은 Google Apps Script에서 처리합니다. DriveX는 HTML·CSS·JavaScript용 정적 호스팅이며 PHP·Python·데이터베이스 서버를 실행하지 않습니다. PC에는 웹 서버를 열지 않습니다. `run_spike_recorder.bat`는 점수 전송 작업만 시작합니다.

## 현재 상태

- DriveX용 단일 파일: `drivex-site/index.html` (스타일·스크립트 모두 포함)
- Google 서버 코드: `cloud/Code.gs`, `cloud/appsscript.json`
- PC 전송 코드: `cloud/sync_scores.py`
- Google 웹 앱 배포와 DriveX 사이트 생성은 아직 완료 전입니다. **파일 업로드만으로 서버나 사이트가 만들어지지는 않습니다.**
- DriveX 생성 화면의 Cloudflare 사람 확인, Google 계정의 배포 승인 및 공개 읽기 공유는 계정 소유자가 완료해야 합니다. 연결된 Drive 도구에는 Apps Script 배포나 `링크가 있는 모든 사용자` 공유 설정 기능이 없습니다.

## 1. Google 클라우드 API 배포 (한 번)

1. https://script.google.com/ 에서 새 프로젝트를 만듭니다. `cloud/Code.gs`의 내용을 `Code.gs`에 붙여 넣습니다.
2. 프로젝트 설정에서 `appsscript.json` 표시를 켜고 제공된 `cloud/appsscript.json`을 붙여 넣습니다.
3. 함수 목록에서 **setup**을 실행하고 Google의 권한 안내를 확인합니다. 공개 홈페이지 폴더를 읽고 수정하며, 별도 비공개 점수 폴더에 기록을 저장하는 데 Drive 접근 권한을 사용합니다.
4. **배포 → 새 배포 → 웹 앱**, 실행 사용자 **나**, 액세스 **모든 사용자**로 배포합니다. Google Workspace 정책상 익명 공개가 금지되어 있으면 관리자의 정책 변경 또는 허용되는 계정이 필요합니다.
5. 배포 후 편집기에서 **publishSite**를 실행합니다. 이 함수는 LOTTOL SV 폴더의 `index.html`에 실제 `/exec` 주소만 넣습니다. 비밀 업로드 키는 별도 비공개 **LOTTOL Ranking Data (private)** 폴더의 `LOTTOL-PC-connection.json`에 저장합니다. 실행 로그에 해당 비공개 파일 링크가 나옵니다.

`Code.gs`를 공개 폴더에 업로드하는 것만으로는 실행되는 Google 웹 앱이 생성되지 않습니다.

## 2. DriveX 홈페이지 생성 (한 번)

1. LOTTOL SV 폴더에 있는 **index.html**을 사용합니다. 수정할 때 같은 파일 ID를 유지해야 `publishSite`가 갱신한 API 주소도 유지됩니다. 중복 `index.html` 파일을 만들지 않습니다.
2. 이 홈페이지 파일의 일반 액세스를 **링크가 있는 모든 사용자 · 뷰어**로 설정합니다. 점수 데이터 폴더와 PC 연결 파일은 비공개로 유지합니다. DriveX의 공개 파일 편집 기능에 필요한 공개 편집 권한은 설정하지 않습니다.
3. https://godrive.my.id/generate.php 에서 **Scan Folder**에 위 폴더 주소를 넣습니다. 폴더 공개를 원하지 않으면 **Manual Input**에서 공개 `index.html` 파일 링크 한 개만 입력합니다.
4. 사이트 이름(예: `lottol-sv`), 본인이 정한 관리 PIN을 입력하고 사람 확인을 완료한 뒤 **Create Website**를 누릅니다. 반환된 실제 사이트 주소를 사용합니다. 이 문서는 생성 전 주소를 가정하거나 예약하지 않습니다.

DriveX가 읽기 권한으로 파일을 가져오지 못하면 공개 편집 권한을 부여하기 전에 서비스 요구 사항을 다시 확인합니다. 소유자 승인 없이 누구나 소스를 수정할 수 있는 권한으로 바꾸지 않습니다.

## 3. 프로그램 연결 (PC마다 한 번)

1. `publishSite`가 만든 **비공개** `LOTTOL-PC-connection.json`을 다운로드합니다.
2. 다운로드한 파일을 가장 상위 폴더의 **configure_cloud.bat** 위로 끌어 놓습니다. 또는 BAT를 실행해 파일 경로를 입력합니다.
3. 평소처럼 **run_spike_recorder.bat**를 실행합니다. G → 게임 선택 → 이름 입력 → 플레이하면 결과를 Google 클라우드에 전송합니다.

설정 파일은 PC의 `cloud/private/connection.json`에만 저장합니다. DriveX 공개 파일이나 이 대화에 업로드 키를 붙여 넣지 않습니다.

## 점수 보관과 장애 처리

- 대기 점수: `server/outbox/*.json` — 폴더 이름은 이전 버전과의 호환을 위해 유지합니다. 이 폴더를 쓰는 것이 로컬 웹 서버를 실행한다는 뜻은 아닙니다.
- 클라우드가 같은 이벤트 ID를 확인한 뒤에만 대기 파일을 지웁니다. 확인 결과는 `server/cloud-sent/`에 남깁니다.
- 연결 실패·배포 전·잘못된 응답이면 대기 파일을 보존합니다. 동일한 이벤트 재전송은 Google 서버에서 중복 기록하지 않습니다.
- Google 점수 폴더는 홈페이지 폴더와 분리되며, 공개 조회 API는 이름·게임·점수·플레이 시간만 반환합니다. 홈페이지에서는 임의 점수를 직접 등록하지 않습니다.
- 개발자 연습 모드·녹화 파일 재생 점수는 랭킹에서 제외합니다.
- 기존 로컬 DB `server/data/rankings.sqlite3`는 삭제하지 않습니다. 과거 DB 기록은 자동으로 클라우드에 이전하지 않습니다.
- Google Apps Script와 Drive의 사용량 제한이 적용됩니다. 대규모 대회용 데이터베이스와 같은 처리량을 보장하지 않습니다.

## 검증과 재생성

```powershell
python cloud/build_site.py
node --test cloud/backend_test.js cloud/client_test.js
python -m unittest discover -s cloud -p test_sync_scores.py
```

실제 Google 권한 승인·배포 및 DriveX 사람 확인 전에는 로컬 자동 검증만 완료할 수 있습니다. 배포 후 `/exec?action=health`가 `app: lottol-ranking`, `status: ok`를 반환하고 실제 게임 결과가 홈페이지에 나타나는지 확인합니다.

근거: [DriveX 지원 범위](https://drvx.net/index.php), [DriveX 폴더 스캔](https://drvx.net/documentation.php), [Google 웹 앱 배포](https://developers.google.com/apps-script/guides/web), [Google Content Service 및 JSONP](https://developers.google.com/apps-script/guides/content).
