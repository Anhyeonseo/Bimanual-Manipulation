# 오른팔 포팅 상태

기준 원본: `Bimanual-Manipulation` main `199e9f9`

## 완료

- 최신 원본 파일 구조 이식
- STM32 프로젝트 이름과 출력물 `stm32_g474_right_arm`으로 분리
- ROS/MoveIt/TF/controller/action 이름을 `right_*`로 변환
- 오른팔 ID 1~6 관절 매핑과 home raw 2048 기록
- ID 1~6 raw 증가 방향 전부 확인 (`docs/test-results/2026-08-02-right-arm-id-directions.md`)
- host와 STM32 양쪽 물리 동작 잠금 추가
- 원본의 protocol, safety, diagnostics, camera, Isaac/MoveIt 시험 포팅
- 한글 저장소 경로의 ROSIDL 문제를 피해 영문 `/tmp` build/install 경로로
  ROS 2 8패키지 빌드와 21개 package test 통과

## 아직 오른팔에서 검증하지 않은 것

- 여섯 축 minimum/maximum raw
- 오른팔별 P gain, torque limit과 load/current threshold
- 오른팔 URDF collision과 q0 실물 일치
- READ_ONLY 장시간 안정성
- 단일 관절, 전체 팔, gripper, cancel/fault 실기
- 오른쪽 손목 카메라와 Pick/Place

## 잠금 해제 조건

다음 항목을 모두 채우기 전에는 `motion_authorized`와
`RIGHT_ARM_MOTION_AUTHORIZED`를 변경하지 않는다.

1. 12 V를 끈 상태에서 ID/배선/기구 간섭 재확인
2. 축별 작은 이동으로 방향 확인
3. 축별 보수적 범위 측정
4. config, firmware와 MoveIt limit 일치 시험 통과
5. host 전체 시험과 STM32 빌드 통과
6. READ_ONLY에서 position/voltage/load/current 정상 확인

원본의 완료 기록은 알고리즘과 절차의 근거다. 오른팔 실기 결과는 이후
`docs/test-results/`에 별도 날짜 파일로 새로 남긴다.
