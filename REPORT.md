# EduOM Report

Name: Eunchae Kim

Student id: 20180297

# Problem Analysis

Object를 저장하기 위한 slotted page 관련 구조를 구현한다.

# Design For Problem Solving

## High Level
Slotted page는 헤더와 데이터(object), slot 영역으로 구성된다. \
Object는 object header와 data 영역으로 구성되며 데이터 영역은 4의 배수로 align되어 있다.\
slot은 object를 구분하고 offset을 저장하여 object를 찾기 위해 사용된다.

Object manager는 다음과 같은 API들로 구성된다.
- EduOM_CreateObject
- EduOM_DestroyObject
- EduOM_CompactObject
- EduOM_ReadObject
- EduOM_NextObject
- EduOM_PrevObject

## Low Level
각 API 함수들이 어떻게 구현되었는지에 대해 설명한다.
- EduOM_CreateObject\
    object 객체를 생성하고, header를 초기화한다.\
    page에 object를 삽입하고, 삽입된 object의 ID를 반환한다.
- EduOM_DestroyObject\
    삭제할 object가 저장된 page를 찾아 available space list에서 삭제한다.\
    삭제할 object에 대응하는 slot을 빈 slot으로 설정하고, page header를 갱신한다.
- EduOM_CompactObject\
    압축하고자 하는 object의 slotNo가 NIL인 경우, 모든 object들을 연속되게 저장한다. 모든 slot의 offset과 object의 길이를 계산하여 slot의 offset을 업데이트한다.

    slotNo가 NIL이 아닌 경우, slotNo에 해당하는 object를 제외한 모든 object들을 연속되게 저장한다. slotNo에 해당하는 object를 마지막 object로 저장한다. 위의 compact 방법을 동일하게 사용한다. 
- EduOM_ReadObject\
    파라미터로 주어진 oid로 object에 접근한다.\
    파라미터로 주어진 offset과 length를 참고하여 포인터를 반환한다.
- EduOM_NextObject\
    curOID에 해당하는 object를 탐색하고, slot array에서 다음 object의 ID를 반환한다.
- EduOM_PrevObject\
    curOID에 해당하는 object를 탐색하고, slot array에서 이전 object의 ID를 반환한다.