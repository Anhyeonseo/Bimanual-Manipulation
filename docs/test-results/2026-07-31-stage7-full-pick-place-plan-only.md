# 단계 7 전체 Pick/Place plan-only 체인

- 날짜: 2026-07-31
- 상태: 전체 Pick/Place plan-only PASS; 물리 Place는 미승인
- 로봇 상태: bridge 정지, 12 V OFF
- MoveIt backend: mock, ROS domain 93

## 목표 지점

Pick target은 검증된 Top-to-base 후보를 재사용한다.

```text
Pick object center = (0.371814352, -0.129674332, 0.006300000) m
Yaw                = -0.034597181 rad
```

첫 Place 후보는 높이와 yaw를 유지한 채 base `+Y` 방향으로 60 mm
이동한다.

```text
Place object center = (0.371814352, -0.069674332, 0.006300000) m
Yaw                 = -0.034597181 rad
```

Place 중심은 검증된 board 사각형과 보수적 workspace 내부에 있다.
base 기준 radial 거리는 약 `0.3783 m`이고, grasp와 pregrasp TCP
높이는 각각 `0.0313 m`, `0.1063 m`이다.

## Plan-only 결과

MoveIt은 Pick pregrasp/grasp, 20 mm lift, Place pregrasp/place에
대해 position-only plan을 모두 성공적으로 반환했다. 모든 전이는
명시적 joint 시작 상태에서 다시 계획했고 관절별 최대 step은
`0.18 rad`였다.

| Phase | Arm 구간 | 결과 |
| --- | ---: | --- |
| q0 → Pick pregrasp | 10 | PASS |
| Pick pregrasp → grasp | 2 | PASS |
| Pick grasp → 20 mm lift | 1 | PASS |
| Lift → Place pregrasp | 2 | PASS |
| Place pregrasp → Place | 2 | PASS |
| Place → retreat | 2 | PASS |
| Place pregrasp → q0 (역순 충돌-free 체인) | 10 | PASS |

조립된 결과는 gripper close/open을 포함해 29개 arm 구간, 총 31개
명령 step으로 구성된다. 최종적으로 arm q0는 전축 0이다. Gripper
목표는 물리적으로 검증된 close/open 위치인 `0.13 / 0.06 rad`다.

## Fail-closed manifest

`tools/assemble_pick_place_plan_only.py`는 독립적으로 다음을
검사한다.

- source 상태와 SHA-256
- `execution_api_used=false`
- `motion_authorized=false`
- `robot_target_available=false`
- 정확한 arm joint 순서
- 성공적이고 연속적인 구간 인덱스
- 기록된 joint delta와 계산된 값 대조
- `0.18 rad` 단계 7 step 한계
- calibration limit과 hash `0x8AD27897`
- Place board, Cartesian workspace, radial, grasp-z, pregrasp-z gate
- 전체 phase 연속성과 최종 q0

Manifest는 `automatic_execution_permitted=false`를 명시적으로
설정한다. 각 phase의 첫 arm 구간과 두 gripper 동작 모두 수동 gate를
요구한다. Assembler는 ROS 실행 Action을 import하지 않는다.

최종 manifest:

```text
artifacts/stage7/2026-07-31/full_pick_place/full_pick_place_plan_only_manifest.json
SHA-256 b293149848c74ef62df7db193fab8e8e54030254f67a29e8853b75a8a494007a
```

## 검증

- Pick pose plan-only: PASS, 184/214 trajectory point
- Place pose plan-only: PASS, 171/203 trajectory point
- 20 mm lift pose plan-only: PASS
- 7개 bounded phase plan 전부: PASS
- Assembler 표적 시험: 6/6 PASS
- 저장소 Python suite: 305/305 PASS
- `git diff --check`: PASS
- 이번 작업 중 로봇 동작: 0

## 필요한 다음 gate

1. 제안된 Place 중심을 물리적으로 표시하고 확인한다.
2. 새로 Pick을 관측해 transform, freshness, confidence, visibility,
   workspace gate를 다시 실행한다. 이번에 저장한 후보는 실제 로봇
   target이 아니다.
3. 매 수동 phase 경계마다 fresh 시작 상태와 diagnostics를 확인하고
   절대 재시도하지 않는 manifest-hash-pinned supervisor를 만든다.
4. 명시적 승인을 받아가며 phase별로 첫 물리 Place를 실행한다.
   무인 실행은 활성화하지 않는다.
5. 전체 경로가 통과한 뒤에만 50회 Pick/Place benchmark를 시작할 수
   있다.
