# Headless 양팔 로봇 조작 시스템

Raspberry Pi 5, ROS 2 Jazzy, STM32G474, 두 대의 SO-ARM101과 세 대의 USB 카메라를 통합하는 멀티카메라 듀얼암 로봇 프로젝트다.

첫 생산 기준선은 왼팔의 재현 가능한 마커펜 Pick and Place다. 오른팔은 방향 검증과 전용 ROS 패키지 구성까지 마쳤고, 왼팔과 같은 단독 수락 gate를 통과시킨 뒤 양팔로 통합한다.

## 핵심 원칙

- Raspberry Pi는 인식, TF, 계획, 상태 머신과 운영을 담당한다.
- STM32는 서보 버스 타이밍, 짧은 setpoint 보간, 제한, watchdog과 fault 처리를 담당한다.
- 부팅과 재연결만으로 로봇이 움직이지 않는다. 기본 상태는 `STANDBY`다.
- 모든 기능은 검증 게이트를 통과한 뒤 다음 단계로 이동한다.
- 성능과 안정성은 추측하지 않고 측정 결과를 남긴다.
- 실제 하드웨어 상수는 측정 전 코드 기본값으로 사용하지 않는다.

## 현재 상태

전체 진행 경과와 단계별 상세는 [ROADMAP.md](docs/ROADMAP.md)를 우선
확인한다. 요약:

- **왼팔(생산 기준선)**: firmware `0x00021800` / calibration `0x8AD27897`
  / capabilities `0x000003FF`. Shoulder P32·Elbow P28로 grasp → 약
  20 mm lift → Place → release → retreat → q0 복귀를 감독하에 1회
  완주했다(단계 7 시운전 100%, 정식 50회/90% 반복은 미실행이라
  `부분 통과`). ROS 2 bridge의 READ_ONLY 차단, MoveIt 표준 Action,
  fail-closed feedback와 무경고 shutdown을 통과했다.
- **오른팔**: ID 1~6 방향 실측, `right_arm_bridge` /
  `so101_right_moveit_config` / `so101_right_isaac_bridge` 패키지 신설,
  q0/좌표 정합 자동 검증까지 완료했다. `motion_authorized: false`가
  host·STM32 양쪽에서 실동작을 이중 차단한다. 축별 raw 범위·torque/PID
  실측이 다음 단계다. 상세: [RIGHT_ARM_PORT_STATUS.md](docs/RIGHT_ARM_PORT_STATUS.md).
- **카메라**: 3대 MJPEG capture, hot-plug 복구, Top eye-to-hand·table–base
  등록 통과. 고정 기하 holdout(배경 2종·조명 3종·반사)에서 legacy
  threshold 검출기는 실패했고, 경량 YOLO-OBB 후보는 miss 0%, false
  positive 0%로 통과했다. Pi 5에서 3카메라+Top OBB 30분 동시 부하도
  통과했다(CPU 평균 35.07%, 온도 최대 50.15°C, throttling 0).
- **Motion**: host 계약(Motion-1), STM32 공통 C queue·보간(Motion-2),
  dormant command route·timing 분석기(Motion-3)까지 완료했다. 실제
  G474 route 연결과 Pi–VCP timing 실측이 다음이다.
- **STM32 프로토콜**: `STATE_FEEDBACK`에 명시적 state가 없는 설계를
  감사해 누락이 아님을 확인하고 `protocol/README.md`에 문서화했다
  (2026-08-03).
- simulation: 왼팔 URDF/Xacro q0 계약, 카메라 장착물, MoveIt mock,
  Isaac Sim 6.0.1 backend 검증 완료.
- 통합 순서: **왼팔 생산 기준선 → 오른팔 단독 동등성 → 양팔 통합**.
  동일한 µrad 관절 규격으로 왼팔 실물, 오른팔 실물, Isaac Sim backend를
  교체 가능하게 구성했다.

## 새 개발 환경 준비

필수 도구:

- STM32CubeIDE 2.2.0 이상과 STM32CubeG4 package
- Python 3.12 이상
- host-side C core를 빌드할 경우 CMake와 C11 compiler

Windows PowerShell에서 Python 환경과 자동 테스트를 준비한다.

```powershell
py -3.12 -m venv .venv-host
.\.venv-host\Scripts\Activate.ps1
python -m pip install -r requirements-host.txt
python -m unittest discover -s tests -p "test_*.py"
python tools\validate_protocol_manifest.py
```

STM32CubeIDE에서는 `firmware/stm32_g474_single_arm`을 Existing Project로 import한다. 상위의 `firmware/stm32_actuator`가 linked resource로 연결되므로 두 디렉터리의 상대 위치를 바꾸지 않는다. `Debug/` 산출물과 개인별 `.launch` 설정은 저장소에 포함하지 않으며 각 PC에서 다시 생성한다.

실제 모터를 사용하는 기본 점검(smoke) 및 동작 시험 도구는 `tools/stm32_*_test.py`에 있다. 전원 차단 수단과 작업 공간을 확보한 뒤 실행한다.

## 공식 MoveIt bringup

backend는 공식 진입점 하나에서 독점 선택한다. 기본값은 실제 장치를 열지 않는
`mock`이며, `stm32`도 명시적으로 허용하기 전에는 READ_ONLY다.

```bash
ros2 launch so101_bringup so101_moveit.launch.py backend:=mock
ros2 launch so101_bringup so101_moveit.launch.py backend:=isaac
ros2 launch so101_bringup so101_moveit.launch.py backend:=stm32
```

동시에 두 bringup을 실행하면 두 번째 실행은 provider node를 시작하기 전에
runtime lock에서 거부된다.

Pi가 STM32 bridge와 serial을 소유하고 워크스테이션이 MoveIt/RViz를 실행하는
분산 구성은 다음 전용 launch를 사용한다. 이 launch는 로컬 hardware provider를
추가로 만들지 않는다.

```bash
ros2 launch so101_bringup external_stm32_moveit.launch.py
```

## 장치별 로컬 설정

공개 저장소에는 개인 NUCLEO의 ST-LINK serial을 넣지 않는다. `single_arm_bridge`의 공개 기본값 `serial_device: auto`는 다음 순서로 장치를 찾는다.

1. `/dev/serial/by-id/usb-STMicroelectronics_STLINK-V3_*-if02`와 일치하는 장치가 정확히 하나면 사용
2. by-id 장치가 없고 `/dev/ttyACM0`가 있으면 fallback으로 사용
3. ST-LINK가 여러 개면 임의로 선택하지 않고 실행을 거부

여러 보드를 연결하거나 장치를 명시적으로 고정하려면 아래 example을 복사한다.

```bash
cd ~/Manipulation/ros2_ws/src/single_arm_bridge/config
cp bridge.local.yaml.example bridge.local.yaml
```

`bridge.local.yaml`에 실제 by-id 경로를 넣고 package를 다시 build하면 기존 `ros2 launch single_arm_bridge bridge.launch.py` 명령이 local 설정을 자동으로 우선 적용한다. `*.local.yaml`은 Git에서 제외되므로 공개 저장소에 장치 식별자가 올라가지 않는다. 자세한 내용은 [로컬 하드웨어 설정](docs/LOCAL_HARDWARE_CONFIG.md)에 기록했다.

## 문서 안내

처음 읽는 순서로 정리했다. 개별 firmware 후보·시험의 원본 증거는 모두
`docs/test-results/`에 날짜별로 있으며, 그 중 결정적인 항목만
[PORTFOLIO_LOG.md](docs/PORTFOLIO_LOG.md)가 요약·링크한다.

**현재 상태와 계획**

- [전체 로드맵과 현재 상태](docs/ROADMAP.md) — 지금 어디까지 왔고 다음이 무엇인지
- [검증 매트릭스](docs/VERIFICATION_MATRIX.md) — 게이트별 통과 상태 한 눈에
- [포트폴리오 작업 기록](docs/PORTFOLIO_LOG.md) — 날짜순 요약 색인
- [오른팔 포팅 상태](docs/RIGHT_ARM_PORT_STATUS.md)
- [프로젝트 헌장](docs/PROJECT_CHARTER.md)

**아키텍처 참고**

- [Pi–STM32 통신 규격](protocol/README.md)
- [Pi 카메라·연산 아키텍처](docs/CAMERA_COMPUTE_ARCHITECTURE.md)
- [STM32 모듈 구조와 Isaac Sim 확장 경계](docs/STM32_MODULAR_ARCHITECTURE.md)
- [하드웨어 인벤토리](docs/HARDWARE_INVENTORY.md)
- [로컬 하드웨어 설정](docs/LOCAL_HARDWARE_CONFIG.md)
- [아키텍처 결정 기록(ADR)](docs/adr/README.md)

**단계별 체크리스트** (`docs/checklists/`)

- [단계 0 하드웨어 검사](docs/checklists/PHASE_0_HARDWARE_BASELINE.md) ·
  [단계 4 Isaac Sim·MoveIt](docs/checklists/PHASE_4_ISAAC_MOVEIT_INTEGRATION.md) ·
  [단계 5 hardware backend](docs/checklists/PHASE_5_LEFT_ARM_HARDWARE_BACKEND.md)
- [단계 8 Top 펜 검출/YOLO-OBB](docs/checklists/STAGE8_TOP_PEN_YOLO_OBB.md) ·
  [단계 9 Policy 배포 번들 계약](docs/checklists/STAGE9_POLICY_DEPLOYMENT_BUNDLE.md)
- [Motion-1/2/3 buffered trajectory 계약](docs/checklists/MOTION_BUFFERED_TRAJECTORY_CONTRACT.md)

**최근 시험 결과** (전체 목록은 `docs/test-results/`)

- [2026-08-03 STATE_FEEDBACK 계약 감사](docs/test-results/2026-08-03-state-feedback-contract-audit.md)
- [2026-08-02 오른팔 ID2~6 방향 검증](docs/test-results/2026-08-02-right-arm-id-directions.md)
- [2026-07-31 감독형 실제 Pick/Place 완주](docs/test-results/2026-07-31-stage7-supervised-pick-place-complete.md)

기타: [단계 0 측정 데이터](hardware/phase0_baseline.json) ·
[제3자 license 고지](THIRD_PARTY_NOTICES.md)

## 저장소 구조

```text
Manipulation/
├── docs/
├── protocol/
├── firmware/stm32_actuator/          # 플랫폼 독립 C core (좌우 공용)
├── firmware/stm32_g474_single_arm/   # 왼팔 CubeIDE board project
├── firmware/stm32_g474_right_arm/    # 오른팔 CubeIDE board project
├── ros2_ws/src/single_arm_bridge/    # 왼팔 Pi binary transport와 ROS 2 bridge
├── ros2_ws/src/right_arm_bridge/     # 오른팔 Pi binary transport와 ROS 2 bridge
├── ros2_ws/src/so101_description/    # 좌우 URDF/Xacro와 mesh
├── ros2_ws/src/so101_moveit_config/  # 왼팔 SRDF, planning, controller contract
├── ros2_ws/src/so101_right_moveit_config/ # 오른팔 SRDF, planning, controller contract
├── ros2_ws/src/so101_bringup/        # mock/Isaac/STM32 통합 launch
├── ros2_ws/src/so101_isaac_bridge/   # 왼팔 MoveIt ↔ Isaac adapter
├── ros2_ws/src/so101_right_isaac_bridge/ # 오른팔 MoveIt ↔ Isaac adapter
├── ros2_ws/src/manipulation_camera_manager/ # V4L2 capture와 phase scheduler
├── isaac_sim/assets/                 # 검증된 Isaac Sim 6.0.1 stage
├── config/
├── hardware/
├── tests/
├── tools/
└── requirements-host.txt
```

Isaac Sim/Isaac Lab 학습과 평가는 데스크탑에서 수행하고, 검증된 policy만
ONNX deployment bundle로 Raspberry Pi 5에 배포한다. 실제 policy의 입력,
출력과 `control_dt`는 Pi 자원 기준선에서 동결한다. 현재 `isaac_sim/`은
단계 4에서 검증한 왼팔 simulation asset을 포함하며, 오른팔은 단독 동등성
gate 뒤에 통합한다.

## 자동 판정

```bash
python3 -m unittest discover -s tests -v
python3 tools/validate_protocol_manifest.py
python3 tools/validate_camera_schedule.py
```

Pi에서 ROS package까지 확인할 때는 다음을 추가로 실행한다.

```bash
cd ~/Manipulation/ros2_ws
source /opt/ros/jazzy/setup.bash
colcon build --symlink-install
colcon test
colcon test-result --verbose
```

## License

자체 작성 코드는 [Apache License 2.0](LICENSE)으로 공개한다. STM32 HAL, CMSIS와 BSP는 각 원본 파일 및 [제3자 license 고지](THIRD_PARTY_NOTICES.md)에 적힌 조건을 따른다.
