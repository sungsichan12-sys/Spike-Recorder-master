# 상대 세기 파워 측정기

선택한 채널에서 힘을 줄 때 신호 진폭이 얼마나 달라지는지 보는 기능이다. 오른쪽 **파워 측정기**에 기준 대비 세기(%), RMS, 신호 파워와 피크를 표시한다. 근전도를 입력한 경우 근전도 신호 진폭의 상대 지표이며, 실제 힘(N), 토크(N·m), 기계적 파워(W), 최대 근력의 백분율(%MVC)을 측정하는 기능은 아니다.

## 사용

1. 근전도 신호가 들어오는 채널을 선택한다. 패널의 입력 종류와 채널 번호를 확인한다.
2. 힘을 뺀 상태에서 **휴식 측정**을 누르고 2초 유지한다.
3. 비교 기준으로 삼을, 편안하게 유지할 수 있는 힘을 준 상태에서 **기준 측정**을 누르고 3초 유지한다. 최대 힘을 낼 필요는 없다.
4. 이 기준 상태가 100%가 된다. 이후 같은 자세와 전극 위치에서 힘을 주고 빼며 상대 세기를 확인한다. 기준보다 큰 신호는 100%를 넘겨 표시한다. 막대의 끝은 100%이며, 숫자는 999%까지 표시하고 그 이상은 `>999%`로 표시한다.

초기화 버튼으로 기준을 지울 수 있다. 입력 장치, 채널, 샘플링률, 필터 설정, 버퍼 또는 파일 탐색 위치가 바뀌면 기준이 자동으로 초기화된다. 전극 위치나 자세를 바꿨을 때에는 사용자가 직접 다시 측정해야 한다. 기준은 실행 세션에만 유지된다.

센서가 없는 상태에서 마이크를 사용하면 **소리의 상대 크기**가 표시된다. 이 경우 패널에도 마이크 입력이라고 표시하며 근육 활동으로 해석할 수 없다. 입력이 멈추거나 일시정지되면 새 데이터 없이 보정 시간이 채워지지 않는다.

## 계산

현재 설정된 필터를 거쳐 기록 버퍼에 들어간 선택 채널의 최근 250 ms를 사용한다. 화면 갱신 간격은 최소 50 ms이며, 실제 새 샘플이 있을 때만 갱신한다.

샘플 `x[i]`에서 창 평균 `mean(x)`를 뺀 `y[i]`를 사용한다.

```
P = mean(y[i]^2)
RMS = sqrt(P)
peak = max(abs(y[i]))

relative_percent = 100 * sqrt(max(P - P_rest, 0) / (P_reference - P_rest))
```

`P_rest`와 `P_reference`는 각 보정 구간의 이동창 파워를 새 데이터 시간으로 가중 평균한 값이다. 보정 시작 후 첫 250 ms는 이전 동작이 이동창에 섞이지 않도록 계산에서 제외한다. 휴식 파워를 빼는 것은 휴식 잡음이 신호와 독립적으로 더해지고 안정적이라고 가정한 앱의 보정 방식이다. 움직임 잡음이나 전극 접촉 변화까지 제거하는 방법은 아니다.

기준 RMS가 `max(휴식 RMS × 1.5, 휴식 RMS + 5 ADC)`보다 작으면 약한 기준으로 거부한다. 16비트 저장 범위 끝에 가까운 샘플(±32760)이 있으면 보정값을 저장하지 않는다. 이는 저장 샘플의 범위 검사이며, 모든 하드웨어의 아날로그 포화를 감지하는 것은 아니다.

전압 보정이 있으면 RMS와 피크는 mV, 파워는 mV²로 표시한다. 없으면 ADC와 ADC²를 사용한다. % 값은 RMS 계열의 상대 진폭으로, 신호 파워의 단순 백분율과 다르다. 진폭이 두 배이면 파워는 네 배이고, 잡음이 무시될 정도로 작을 때 상대 세기는 약 두 배가 된다.

창이 충분히 크면 최근 10초의 상대 세기 기록도 보인다. 그래프 범위는 0–150%이며 그 이상의 값은 그래프 위쪽에서 잘리지만 수치 계산에는 반영된다.

## 참고 논문과 적용 범위

- Clancy EA, Hogan N. **Single site electromyograph amplitude estimation.** IEEE Transactions on Biomedical Engineering. 1994;41(2):159–167. DOI: [10.1109/10.284927](https://doi.org/10.1109/10.284927). [저자 제공 원문](https://users.wpi.edu/~ted/full_text/00284927.pdf). 이 실험은 245 ms 이동 RMS를 포함한 근전도 진폭 추정 방법을 평가했다. 앱은 이동 RMS 개념을 참고하고 구현 기본 창을 250 ms로 정했다. 논문의 whitening 필터 전체를 재현한 구현은 아니다.
- Besomi M et al. **Consensus for experimental design in electromyography (CEDE) project: Amplitude normalization matrix.** Journal of Electromyography and Kinesiology. 2020;53:102438. DOI: [10.1016/j.jelekin.2020.102438](https://doi.org/10.1016/j.jelekin.2020.102438). [저자 소속기관 원문](https://research.vu.nl/ws/portalfiles/portal/122516008/Consensus_for_experimental_design_in_electromyography_CEDE_project.pdf). 기준 진폭에 대한 정규화 원칙과 측정 맥락에 맞는 기준 선택을 참고했다. 앱의 사용자가 정한 기준은 표준화된 MVC 측정이 아니다.

250 ms 창, 휴식 2초, 기준 3초, 최소 기준 차이, 잡음 차감 및 입력 중단 처리 기준은 이 앱의 구현 선택이다. 논문이 이 조합의 정확도나 특정 SpikerBox의 근력 측정을 검증했다는 뜻은 아니다.

## 검증

- `tests/power_meter_tests.cpp`: 알려진 사각파·사인파, DC 오프셋, 파워의 제곱 관계, 기준 0%·100%, 기준 초과값, 약한 기준, 범위 초과, 일시정지·입력 중단·초기화 등 계산 검사.
- `tests/core_regression.cpp`: 실제 WAV 디코더와 RecordingManager 버퍼를 통해 휴식·기준·중간 세기 샘플을 읽고 예상 상대값과 비교하는 통합 검사. 기존 녹음·필터·분석 검사도 함께 수행한다.
- 실제 센서와 인체 신호의 정확도 검증, 현 UI 화면 조작 검증은 수행하지 않았다. 컴퓨터 유즈와 앱 창을 여는 검사는 사용하지 않는다.

프로젝트 폴더에서 계산 검사를 다시 실행하려면:

```powershell
C:\msys64\mingw32\bin\g++.exe -std=gnu++11 -O2 -Wall -Isrc tests/power_meter_tests.cpp -o tests/power_meter_tests.exe -static
.\tests\power_meter_tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_core_checks.ps1
```
