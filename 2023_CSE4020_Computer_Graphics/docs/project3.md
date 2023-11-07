# BVH Viewer

> Computer Graphics Project3 - 한양대학교 컴퓨터소프트웨어학부 2017029343 김기환
> 

## Which requirements you implemented

A. Manipulate the camera in the same way as in Project1 using your Project1 code

- 카메라의 이동, Grid plane을 그리는 것은 Project1에서 진행한 것과 동일하게 구현.

B. Load a bvh file and render it

- drag-and-drop으로 bvh 파일이 window에 drop되면 drop_callback 함수 수행
- drop_callback 함수 내부에서 parsingBVH 함수를 수행하면서 drop된 bvh 파일을 parsing 하면서 Node 클래스를 기반으로 각 관절들의 계층구조를 형성
- infoBVH 함수에서 drop된 bvh 파일에 담긴 정보를 다음과 같이 터미널에 출력.
    
    ![%EC%BA%A1%EC%B2%98](https://github.com/study-kim7507/HYU/assets/63442832/0108b79c-7cfc-4469-ba95-973b83d9eebb)
    
- Line Rendering
    - 파싱하여 생성된 계층구조를 바탕으로 main 함수 내부에서 각 관절의 global_transform을 바탕으로 vao를 생성하고 이를 이용하여 Line Rendering을 진행
    - 처음 bvh파일이 drop되면 파싱하여 얻은 모션데이터를 반영하지 않고 Rest Pose를 렌더링하도록 main 함수에서 분기를 나눔.
        
        ![1](https://github.com/study-kim7507/HYU/assets/63442832/555b8b24-fbc6-4b97-a1bd-e5518ca1770c)
        
    - 이후, spacebar 키가 입력되면 모션데이터를 적용하여 계산된 global_transform을 바탕으로 vao를 만들어 움직임을 구현함.
- Box Rendering
    - Box를 렌더링하는 vao를 만들고, 모션데이터를 적용한 global_transform을 각 관절을 이루는 box에 적용하여 관절마다 하나의 box를 가지고, 움직일 수 있도록 구현
    - Lab8의 코드를 참조하여 관절을 이루는 각 box마다 Phong Shading과 Phong Illumination을 적용.
        
        ![2](https://github.com/study-kim7507/HYU/assets/63442832/d7d4e0fd-4b45-44ea-b149-f0f00b3fff63)
        
    
- A hyperlink to the video uploaded to Internet video streaming services (such as
YouTube and Vimeo) by capturing the animating hierarchical model as a video (10
pts).

[컴퓨터 그래픽스 Project3 2017029343 김기환](https://youtu.be/wsELO-GczIE)
**Youtube Link**

## Trouble Shooting

- 기존 계획은 Line Rendering과 Box Rendering 모두 하나의 vao를 바탕으로 uniform 변수를 적용하여 렌더링하여 구현하려 했지만, 각 관절의 로컬프레임이 부모의 로컬프레임을 기준으로 계산이 되다보니, 관절마다 방향이 달라 렌더링이 이상하게 되는 문제가 발생
    - 따라서 Line Rendering은 각 프레임마다 vao를 새로 만드는 식으로 구현. → 매 프레임마다 vao를 만드는 것은 비효율적임 해결해야할 문제.
    - Box Rendering은 관절마다 로컬프레임이 다르게 계산이 되어 렌더링이 이상하게 되어 모든 관절의 shape_transform을 동일하게 구현함. → 해결해야할 문제 .
- BVH 파일마다 좌표의 최대값과 최소값, 모션데이터의 최대값과 최소값이 다른 문제로 인해, 동일한 범위의 좌표내에서 렌더링하도록 offset(좌표)의 값과 모션데이터의 값을 max 값, radians을 이용하여  최대한 비슷한 범위 내에서 렌더링되도록 구현함.
