# 검증 게이트 기반 전체 로드맵

## 진행 규칙

- 각 단계의 검증 결과를 `docs/PORTFOLIO_LOG.md`에 요약으로 기록하고, 원본
  증거는 `docs/test-results/`에 날짜별 파일로 남긴다.
- 수치 결과는 `benchmark/results/`에 원본과 요약을 분리해 보관한다.
- 실제 하드웨어 활성화는 이전 단계의 완료 조건을 충족한 뒤 진행한다.
- 이 문서는 "지금 어디까지 왔고 다음이 무엇인지"만 담는다. 과거 시행착오의
  세부 기록은 `PORTFOLIO_LOG.md`와 `test-results/`를 따로 확인한다.

## 현재 상태 (2026-08-03)

- **왼팔**: firmware `0x00021800`, calibration `0x8AD27897`,
  capabilities `0x000003FF`. Shoulder P32 / Elbow P28로 grasp → 약 20 mm
  lift → Place → release → retreat → q0 복귀를 감독하에 1회 완주했다
  (단계 7 시운전 체크리스트 100%, 정식 50회/90% 반복 기준은 미실행이라
  `부분 통과`). 서보 UART 재동기화, 5분 무동작 heartbeat, fault
  injection·6축 복구는 통과했다.
- **오른팔**: ID 1~6 매핑과 raw 증가 방향을 실측 확인했다
  (`docs/test-results/2026-08-02-right-arm-id-directions.md`).
  축별 min/max raw·P gain·torque limit은 아직 미실측이며, `right_arm_bridge`
  / `so101_right_moveit_config` / `so101_right_isaac_bridge` 패키지를
  왼팔 프로덕션 코드와 분리해 새로 만들었다. `motion_authorized: false`가
  host와 STM32(`firmware/stm32_g474_right_arm`) 양쪽에서 실제 동작을
  이중 차단한다. 세부 상태는
  [RIGHT_ARM_PORT_STATUS.md](RIGHT_ARM_PORT_STATUS.md) 참고.
- **Top 카메라**: 고정 기하에서 배경 2종·조명 3종·반사를 포함한 holdout
  18장을 승인했고 legacy threshold 검출기는 실패(miss 100%)했다. 이후
  경량 YOLO-OBB 후보가 같은 holdout에서 miss 0%, false positive 0%를
  통과했으며 Pi 5에서 3카메라 동시 30분 runtime gate도 통과했다.
- **Pi 5 자원 기준선**: 3카메라 + STM32 READ_ONLY 30분 동시 부하에서 CPU
  평균 7.94%, 온도 최대 40.8°C, heartbeat/feedback 오류 0을 확인했다.
  Top OBB를 포함한 3카메라 30분 동시 부하도 통과했다. 실제 policy ONNX는
  아직 로컬에 없어 policy shadow gate는 미실행이다.
- **Motion(다관절 buffered trajectory)**: host 계약(Motion-1)과 STM32
  공통 C queue/보간(Motion-2), dormant command route와 timing 분석기
  (Motion-3)까지 완료했다. 실제 G474 route 연결과 Pi–VCP timing 실측은
  아직이다.
- **STM32 프로토콜 계약 감사(2026-08-03)**: `STATE_FEEDBACK`에 명시적
  state 필드가 없는 것이 계약 누락인지 점검했다. 원본 설계대로
  `ARM_RESPONSE`/`SETPOINT_STATUS`가 원시 state를, `STATE_FEEDBACK`의
  `status_code`/`stop_latched` 조합이 나머지 전이를 나타내는 의도된 설계임을
  확인했다. 상세:
  [state-feedback 계약 감사](test-results/2026-08-03-state-feedback-contract-audit.md),
  [protocol/README.md 7절](../protocol/README.md).

### 아직 채택하지 않은 항목

- single-point Action을 이어 붙인 정착형 실행은 생산용 연속 trajectory가
  아니다. G474 binary route와 ROS multi-point Action은 아직 연결 전이다.
- Place 접촉 offset 재계측, 왼쪽 손목 카메라 eye-in-hand, 50회 반복
  benchmark가 남아 있다.
- 오른팔·양팔 동작은 formal gate를 통과하지 않았다.
- 실제 Isaac policy ONNX의 입력·출력, control_dt는 아직 deployment
  contract로 동결하지 않았다.

## 다음 결정 원칙

1. 왼팔을 재현 가능한 단일 팔 기준선으로 먼저 완성한다.
2. 오른팔에 왼팔과 같은 검증 절차를 그대로 적용해 단독 동등성을 만든다.
3. 두 팔이 각각 단독 기준선을 통과한 뒤에만 양팔 동시·공유 영역을 통합한다.
4. Isaac Sim/Isaac Lab 학습은 데스크탑에서 수행하고, 검증된 policy만 ONNX
   deployment bundle로 Raspberry Pi 5에 배포해 실제 inference한다.
5. MoveIt은 전역 경로와 충돌 검사를 담당하고, policy는 관절 보정값 또는
   제한된 Cartesian residual만 출력한다. STM32나 servo raw 명령을 직접
   우회하지 않는다.
6. Top 카메라는 전역 탐색, 손목 카메라는 접근 직전 상대 정렬을 담당한다.
7. stale observation이나 deadline miss가 있으면 이전 action을 반복하지
   않고 fail-closed 한다.

상세 결정 근거는 [ADR-0012](adr/0012-arm-integration-and-pi-policy-deployment.md)를
따른다.

## 목표 실행 구조

~~~text
Desktop
└─ Isaac Sim/Isaac Lab 학습·평가
   └─ policy.onnx + manifest + calibration/normalization hash

Raspberry Pi 5
├─ 3-camera capture와 phase scheduler
├─ 공통 observation adapter
├─ Top/손목 perception
├─ policy.onnx inference
├─ MoveIt 전역 경로 또는 검증된 trajectory
├─ action safety supervisor와 command arbiter
└─ STM32 bridge

STM32
├─ servo bus 시간축과 bounded interpolation
├─ heartbeat/watchdog
├─ position/read failure 진단
└─ HOLD, physical DISABLE과 latched stop
~~~

## 전체 단계

각 단계는 목표 → 구현 항목 → 완료 조건 순서다. 진행 과정의 세부 기록은
`PORTFOLIO_LOG.md`와 해당 `test-results/` 링크를 따른다.

### 단계 0 — 하드웨어 기준선과 요구사항 동결

- 서보 12축의 ID, 방향, raw 범위와 상태값(feedback) 확인
- 전원, 배선, adapter, MCU, 카메라 인벤토리 완성
- 완료 조건: 미확정 하드웨어 상수 목록과 측정 계획이 모두 존재
- **상태**: 왼팔 완료. 오른팔은 ID/방향만 완료, raw 범위는 미실측.

### 단계 1 — 저장소·인터페이스·모의 장치(Mock) 골격

- SO-101 Xacro, SRDF, ros2_control 모의 하드웨어, STANDBY/ARMING 상태 머신
- 완료 조건: 새로 내려받은 저장소에서 build, test, 모의 장치 실행 성공
- **상태**: 완료.

### 단계 2 — STM32 제어 기반

- ST-LINK VCP 바이너리 통신 규격, CRC, sequence, heartbeat, fault latch
- 단일 팔 UART와 6축 동시 쓰기/읽기, 크기가 제한된 trajectory buffer
- 완료 조건: 단일 팔 통신·동작·SAFE_STOP 실기와 protocol 자동 시험 통과
- **상태**: 완료. 양팔용 독립 UART와 8시간 반복 시험은 단계 11에서 추가.

### 단계 3 — Pi 카메라 관리와 성능 기준선

- 카메라 3대 수집 thread, 최신 frame 1장 buffer, 상태 기반 scheduler
- 완료 조건: 카메라와 STM32를 동시에 사용해도 제어 heartbeat 위반이 없음
- **상태**: 완료.

### 단계 4 — MoveIt/Isaac Sim 기구학 검증

- 왼팔 planning group, 충돌 모델, workspace, Isaac Sim URDF·카메라 mount
- 완료 조건: 모의 하드웨어와 Isaac 환경에서 대표 trajectory 검증
- **상태**: 완료. q0는 사진 기반 1차 정합 후 외부 계측(eye-to-hand)으로
  재정밀화했다. 상세:
  [단계 4 통합 결과](test-results/2026-07-24-isaac-moveit-left-arm-integration.md),
  [wrist-flex q0 보정](test-results/2026-07-30-wrist-flex-q0-metrology-refinement.md).

### 단계 5 — 실제 왼팔 제어

- MoveIt 표준 Action → `single_arm_bridge` → STM32 → 왼팔, single-point 계약
- 완료 조건: 반복 trajectory와 통신 단절 시험 통과
- **상태**: 완료. 상세: [단계 5 실기 결과](test-results/2026-07-25-phase5-stm32-read-only.md).

### 단계 6 — Top 카메라 인식(Perception)

- intrinsic calibration, 작업대 homography, 펜의 `x, y, yaw`와 신뢰도 출력
- 완료 조건: 위치 오차가 grasp 허용 오차 이내
- **상태**: 완료. board-relative 위치 RMSE `6.34 mm`, yaw RMSE `1.9 deg`.
  상세: [Top 물체 좌표 검증](test-results/2026-07-30-top-object-ground-truth-validation.md).

### 단계 7 — 재현 가능한 Pick and Place

- 왼팔 상태 머신, grasp/place 검증, 50회 반복 시험
- 완료 조건: Pick/Place 각각 90% 이상, 비명령 동작·충돌 0회
- **상태**: `부분 통과`. Top–base 등록, base-frame shadow target, 물리
  reachability 감사를 거쳐 operational raw range를 재보정했다. 여러
  firmware 후보(heartbeat RX starvation, torque/settling, UART
  재동기화)를 순차로 원인 분리·수정한 끝에 `0x00021800`으로 grasp → 약
  20 mm lift → Place → release → retreat → q0 복귀를 감독하에 1회
  완주했다. 정식 완료 조건인 50회/90% 반복은 아직이며, 그 전에 단계 8의
  buffered trajectory로 single-point 연쇄를 대체한다. 상세 경과는
  `PORTFOLIO_LOG.md`, 최종 결과는
  [감독형 실제 Pick/Place 완주](test-results/2026-07-31-stage7-supervised-pick-place-complete.md).

### 단계 8 — 왼팔 생산 기준선과 Visual Servo

- single-point 연쇄를 multi-point/buffered trajectory로 교체(시간축,
  queue, cancel, soft-abort, SAFE_STOP 계약을 실물에서 검증)
- Pick/Place 접촉 Z 분리 보정, 대리석 무늬·반사 배경에서도 견고한 펜 검출
- 왼쪽 손목 카메라 eye-in-hand와 마지막 수 cm bounded visual residual
- 완료 조건: 배경·조명만 다른 환경에서도 10회 예비 + 50회 Pick/Place
  각각 90% 이상, 비명령 동작·충돌 0회, 단계 7 정식 반복성 gate 통과
- **상태**: `부분 통과`. Motion-1(host 계약)·Motion-2(STM32 queue/보간)·
  Motion-3(dormant command route·timing 분석기)를 완료했다. G474 route
  연결과 실기 검증이 남았다. 펜 검출은 YOLO-OBB 후보가 holdout miss 0%로
  통과했다. 상세: [Motion 체크리스트](checklists/),
  [Top 펜 holdout 결과](test-results/2026-08-02-top-pen-holdout-legacy-baseline.md).

### 단계 9 — Pi 5 세 카메라·Policy Runtime·Headless 기준선

- 압축 latest-frame slot과 phase scheduler, versioned ONNX policy 배포
- ARM64 Release build, systemd/udev/journald/watchdog, 30분→8시간→24시간 부하
- 완료 조건: 반복 부팅 STANDBY, heartbeat 위반 0회, stale 출력 100% 차단
- **상태**: `부분 통과`. camera-only 30분과 Top-OBB 포함 3카메라 30분
  동시 부하를 통과했다. 실제 ONNX policy가 아직 없어 shadow mode·8시간
  soak·재부팅 반복은 미실행이다. `config/policy_deployment_contract.json`으로
  model·observation·action·runtime 계약을 fail-closed로 먼저 고정했다.
  상세: [Pi runtime camera-only 30분](test-results/2026-08-02-pi-runtime-camera-only-30m.md).

### 단계 10 — 오른팔 단독 동등성

- 오른팔 6축 ID·방향·raw limit·q0·전원·온도·PID/torque readback 확정
- 오른팔 URDF/MoveIt/Isaac FK와 encoder→ROS→모델 parity 검증
- READ_ONLY physical disable, 무동작, 단일 축 격리 이동, cancel/fault 복구
- 오른쪽 손목 카메라 eye-in-hand와 오른팔 단독 Pick/Place 반복 시험
- 완료 조건: 왼팔과 동일한 하드웨어·모델·안전·태스크 수락 기준을 오른팔이
  독립적으로 통과
- **상태**: 진행 중. ID 1~6 방향 확인, `right_arm_bridge`/
  `so101_right_moveit_config`/`so101_right_isaac_bridge` 패키지 구성,
  q0/좌표 정합 자동 검증(`tests/test_right_arm_q0_contract.py`)까지
  완료했다. 축별 raw 범위·torque·PID 실측과 READ_ONLY 이후 단계는 남아있다.
  상세: [RIGHT_ARM_PORT_STATUS.md](RIGHT_ARM_PORT_STATUS.md).

### 단계 11 — 양팔 통합과 Policy 권한 확대

- 왼팔·오른팔 단독 기준선이 모두 통과한 뒤 dual planning group과 공유
  collision scene 활성화, 개별/공유 작업 영역 분리
- 한 팔 fault 시 양팔 동시 정지, policy는 저장 데이터 평가 → Pi shadow
  mode → 제한된 residual 순서로만 권한 확대
- 완료 조건: 충돌 0회, 연동 정지 100%, policy의 baseline 대비 수치상 개선
- **상태**: 미실행. 단계 8·10 완료가 선행 조건이다.

### 단계 12 — 수건 접기와 최종 포트폴리오

- segmentation/keypoint 기반 수건 상태 인식, 양팔 grasp와 fold 상태 머신
- 완료 조건: 환경·카메라·모델·policy bundle을 고정한 재현성 시연
- **상태**: 미실행.

## 바로 다음 작업

1. Motion-3까지 완료한 host/STM32 buffered 계약을 실제 G474 route에
   연결하고 Pi–VCP timing을 실측한다.
2. ROS multi-point Action adapter를 붙이고 mock/plan-only 검증 후 제한
   실기로 확장한다.
3. 오른팔은 축별 min/max raw부터 RIGHT_ARM_PORT_STATUS.md의 잠금 해제
   조건 순서대로 진행한다.
4. 실제 policy ONNX 확보 후에만 Pi shadow inference로 넘어간다.

Git 조작(commit/push)은 사용자가 직접 수행한다.
