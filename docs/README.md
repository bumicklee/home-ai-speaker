# docs/

이 스피커를 만들 때 쓴 기능표·설계서와 앱 화면 그림이다. 독자는 책 2장에서 자기 문서를 직접 만들고, 막히면 여기 있는 완성본 문서와 비교한다.

| 파일 | 책 | 내용 |
|:--|:--|:--|
| `01-plan/ai-speaker.features.md` | 2장 | 기능표. 코드 주석·설계서·책의 기능 ID(`VOI-03`, `TLG-08` 등)는 이 표에서 찾는다 |
| `design.md` | 2장 이후 전체 | 설계서: 핀표, 컴포넌트, BLE·LAN 규격, 파티션, sdkconfig. 코드 주석의 "설계서 N.N절"은 이 문서다 |
| `design-voice-saver-mode.md` | 17·18장 | 절약 모드와 AI 고르기 설계 |
| `design-notes-schedule.md` | 16장 | 메모·일정·알림 설계 |
| `dev-environment.md` | 1장 | 개발 환경(버전표, 설치 명령, 자주 나는 문제) |
| `AI-PA 앱 화면.pdf` | 11장, 부록 F | 완성된 앱 화면 25개 (한 쪽에 화면 하나) |
| `Jarvis 스피커 앱 디자인.html` | 11장 | 처음 디자인 캔버스를 내보낸 파일 (화면 11개) |

- 날짜와 "Phase N" 표기는 만들 때의 기록을 그대로 둔 것이다. 책에서는 "N장"으로 부른다.
- 기획서, 단계별 작업 일지, 원인 분석(RCA) 문서는 넣지 않았다. 같은 내용은 책의 해당 장에 있고, 코드 주석도 책의 장·절을 가리킨다.
- `HANDOFF.md`(인수인계)는 넣지 않았다. 다음 Claude Code 세션이 지금 상태로 읽는 문서라, 2장에서 내 저장소에 직접 만든다.
- 문서 양식은 `.claude/skills/speaker-method/templates.md`에 있다(`/speaker-method plan`, `design`, `handoff`, `rca`).
