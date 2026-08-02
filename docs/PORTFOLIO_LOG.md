# 포트폴리오 작업 기록 (요약 색인)

날짜순 요약 색인이다. 각 항목의 수치·SHA·구조체 단위 상세 증거는 연결된
`docs/test-results/*.md`에 있다. 이 문서는 "언제 무엇을 왜 했는지"를
빠르게 훑기 위한 것이고, 재현에 필요한 원본 기록은 test-results가
우선한다. 현재 구현 상태는 [ROADMAP.md](ROADMAP.md)와
[검증 매트릭스](VERIFICATION_MATRIX.md)를 우선해서 확인한다.

## 2026-07-12 — 프로젝트 착수와 초기 골격

- 프로젝트 헌장·하드웨어 인벤토리·전체 로드맵·핵심 ADR 작성.
- `hardware/phase0_baseline.json` + `tools/validate_phase0.py`로 서보
  기준선 측정의 누락·모순을 fail-closed로 검출하는 골격을 만들었다.
- Pi–STM32 통신 규격 초안(COBS+CRC-32C, micro-radian 단위, 메시지 ID
  manifest)을 확정하고 자동 검증기를 붙였다.
- Raspberry Pi 카메라 3대 역할과 작업 상태별 연산 한도(`CAMERA_COMPUTE_ARCHITECTURE.md`)를
  정의했다.
- 증거: `tests/test_validate_phase0.py`, `protocol/README.md`,
  `tools/validate_protocol_manifest.py`, `config/camera_schedule.json`.

## 2026-07-21 — 카메라 선택적 decode와 STM32 제어 격리

- phase별로 필요한 카메라만 decode하는 scheduler를 구현하고, STM32
  READ_ONLY bridge와 120초 동시 실행으로 제어 격리를 확인했다.
- 결과: decode 실패 0, `/joint_states` 5.008 Hz 유지, CPU 평균 6.38%.
- 증거: [phase decode latency](test-results/2026-07-21-camera-phase-decode-latency.md),
  [decode-control load](test-results/2026-07-21-camera-decode-control-load.md).

## 2026-07-24 — 왼팔 MoveIt·Isaac Sim 6.0.1 통합

- 왼팔 URDF/Xacro·SRDF·mock controller와 Isaac articulation을 같은 joint
  이름/방향/q0 계약으로 연결했다. 실제 servo 동작 0회.
- 결과: mock·Isaac arm/gripper Plan/Execute 전부 성공, home 오차 약
  `0.0097 rad`.
- 증거: [단계 4 통합 결과](test-results/2026-07-24-isaac-moveit-left-arm-integration.md).

## 2026-07-25 — 단계 5 실제 왼팔 MoveIt·STM32 통합

- Pi가 STM32 serial을 단독 소유하고 워크스테이션 MoveIt이 표준 Action으로
  실제 왼팔을 제어하는 초기 B안(single-point)을 실기로 검증했다.
- 결과: cancel/SAFE_STOP/recovery/reconnect stale-goal 방지 모두 통과,
  `/joint_states` 5.000 Hz.
- 증거: [단계 5 실기 결과](test-results/2026-07-25-phase5-stm32-read-only.md).

## 2026-07-30 — 단계 6·7 준비: Top 인식과 작업대 등록

- 검은 펜 Top 좌표 검증(위치 RMSE `6.34 mm`, yaw RMSE `1.9 deg`)과
  base-frame shadow target, table–base 등록(GridBoard PnP)을 통과했다.
  초기 `118.216 mm` 불일치는 폐기된 체스보드 pose와의 혼합 비교 오류로
  확인해 정정했다.
- torque-disabled 관측(600초)으로 Shoulder/Elbow 물리 raw 범위를
  재측정해 보수적 operational limit(`1988..3766` / `627..2258`)을
  채택했다.
- 증거: [Top 물체 좌표 검증](test-results/2026-07-30-top-object-ground-truth-validation.md),
  [현재 작업대–base 등록](test-results/2026-07-30-current-table-base-registration.md),
  [물리 범위 재검증](test-results/2026-07-30-physical-range-revalidation.md).

## 2026-07-30 ~ 2026-07-31 — 단계 7 firmware 반복과 감독형 Pick/Place 완주

Pregrasp/grasp를 작은 구간(최대 `0.18~0.30 rad`)으로 나눠 plan-only로
먼저 검증한 뒤 제한 실기로 확장하는 동안, 아래 순서로 firmware 원인을
분리하고 수정했다.

| firmware | 문제 | 조치 |
|---|---|---|
| `0x00020A00` | soft-abort 후 stop latch 재발 | `0x00020B00`으로 rollback 후 재작업 |
| `0x00020C00` | Shoulder torque 부족으로 소각도 soft-abort | torque `650/550→780/650` |
| `0x00020D00` | torque-limit 미검증(P/D/I만 readback) | 16비트 torque readback fail-closed 추가 |
| `0x00020E00` | 소각도 이동에서 stop latch 재현 | settling/diagnostics 재설계 |
| `0x00020F00` | heartbeat RX starvation(1바이트 polling) | bounded RX drain + acknowledged heartbeat |
| `0x00021000` | LPUART 하드웨어 byte 유실 | interrupt RX + ring buffer + cooperative step |
| `0x00021100` | READ_ONLY 재연결 DISABLE 오판정 | DISABLE을 멱등적 물리 안전 계약으로 재정의 |
| `0x00021800` | (최종) | 서보 UART 재동기화·완전 복구, 확장 failure cause |

- `0x00021800` + calibration `0x8AD27897` + capabilities `0x000003FF`로
  Shoulder P32/Elbow P28 설정, grasp → 약 20 mm lift → Place(5 mm Z
  보정 2회) → release → retreat → 11구간 q0 복귀를 감독하에 1회 완주했다.
  최종 q0 오차 Wrist Roll `0.007670 rad`.
- 판정: 단계 7 감독형 시운전 체크리스트 100%. 정식 완료 조건(50회/90%)은
  미실행이라 `부분 통과`. 다음은 single-point 연쇄를 buffered
  trajectory로 교체하는 단계 8.
- 상세 증거(각 firmware 후보의 SHA·바이트 단위 diagnostics 포함):
  [Shoulder 근본 원인·0x00020E00](test-results/2026-07-30-stage7-shoulder-root-cause-remediation.md),
  [0x00020E00 물리 거절](test-results/2026-07-31-stm32-0x00020e00-rejected-heartbeat-rx.md),
  [0x00020F00 후보](test-results/2026-07-31-stm32-0x00020f00-heartbeat-ack-candidate.md),
  [0x00021000 후보](test-results/2026-07-31-stm32-0x00021000-interrupt-buffered-cooperative-motion-candidate.md),
  [감독형 실제 Pick/Place 완주](test-results/2026-07-31-stage7-supervised-pick-place-complete.md).

## 2026-08-01 — 현재 분기점, Top 카메라 재배치, 오른팔 복구 보고

- 재배치한 Top 카메라 영상은 정상(`640×480 rgb8`, sharpness `87.93`)이지만
  legacy threshold 검출기가 대리석 무늬·반사에서 fail-closed됐다. 카메라
  문제가 아니라 검출기 일반화 문제로 분리했다.
- 사용자가 오른팔 정상 동작을 확인했다. 정식 수락은 별도 gate(단계 10)로
  분리하고, 통합 순서를 **왼팔 생산 기준선 → 오른팔 단독 동등성 → 양팔
  통합**으로 확정했다.

## 2026-08-02 — Pi 5 자원 기준선, 펜 검출 holdout, 오른팔 방향 검증

- 3카메라(Top 6 Hz, 손목 각 5 Hz) + STM32 READ_ONLY 30분 동시 실행: CPU
  평균 7.94%, 온도 최대 40.8°C, heartbeat/feedback/reconnect 오류 0.
- 시연 환경 조건(배경 2종·조명 3종·반사) holdout 18장에서 legacy
  검출기는 miss 100%로 실패했다. 이후 학습한 YOLO-OBB 후보는 같은
  holdout에서 miss 0%, false positive 0%를 통과했고, Pi 5 3카메라+Top
  OBB 동시 30분 부하도 통과했다(CPU 평균 35.07%, 온도 최대 50.15°C).
- Motion-1(host 계약)·Motion-2(STM32 queue/보간)·Motion-3(dormant
  command route·timing 분석기)를 완료했다. 실제 G474 route 연결은 이후 gate.
- 오른팔 서보 ID 2~6의 raw 증가 방향을 실측 확인했다.
- 증거: [Pi runtime camera-only 30분](test-results/2026-08-02-pi-runtime-camera-only-30m.md),
  [Pi runtime Top-OBB 30분](test-results/2026-08-02-pi-runtime-top-obb-30m.md),
  [Top 펜 holdout·legacy 기준선](test-results/2026-08-02-top-pen-holdout-legacy-baseline.md),
  [오른팔 ID2~6 방향](test-results/2026-08-02-right-arm-id-directions.md).

## 2026-08-03 — 오른팔 패키지 통합과 STM32 프로토콜 계약 감사

- 별도로 진행하던 오른팔 캘리브레이션 작업(`Bimanual-Manipulation-Righthand`)에서
  검증된 결과물을 이 저장소로 병합했다: `right_arm_bridge`,
  `so101_right_moveit_config`, `so101_right_isaac_bridge` 패키지 신규
  추가, `so101_right.urdf.xacro`, 오른팔 firmware 포트
  (`firmware/stm32_g474_right_arm`). 왼팔 프로덕션 코드는 건드리지 않았다.
- `ArmCalibration`에 `motion_authorized` 필드를 추가해 calibration 파일
  자체에서 물리 동작 허가 여부를 표현하도록 개선했다. 왼팔은 기존 실기
  검증을 근거로 `true`, 오른팔은 축별 range 미실측 상태라 `false`로
  이중 잠금했다.
- `tests/test_right_arm_q0_contract.py` 7건 전부 통과로 오른팔
  URDF/SRDF/calibration/Isaac mapping의 상호 정합을 확인했다.
- `STATE_FEEDBACK`에 명시적 state 필드가 없는 것이 계약 누락인지 감사한
  결과, 원본 설계가 `ARM_RESPONSE`/`SETPOINT_STATUS`의 원시 state와
  `status_code`/`stop_latched` 조합으로 상태를 추론하도록 의도돼 있음을
  확인했다(누락 아님). `protocol/README.md` 7절에 문서화했다.
- 증거: [state-feedback 계약 감사](test-results/2026-08-03-state-feedback-contract-audit.md),
  [RIGHT_ARM_PORT_STATUS.md](RIGHT_ARM_PORT_STATUS.md).

---

## 기록 템플릿 (신규 항목 추가 시)

```markdown
## YYYY-MM-DD — 제목

- 목표/변경 1~3줄
- 결과 1~3줄 (핵심 수치만; 전체 수치·SHA는 test-results에)
- 판정: 통과/부분 통과/실패/차단, 다음 gate
- 증거: [설명](test-results/파일명.md)
```
