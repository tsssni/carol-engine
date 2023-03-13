# CarolRenderer

## Dependencies:

- *Assimp* 5.2.5
- *DirectXTex* 2022.12.18.1
- *D3D12 Agility* 1.608.2
- *DirectX Shader Compiler* 1.7.2212.23

## Rendering Pipeline

![pipeline](https://i.lensdump.com/i/T7S1XF.png)

## Features:

- **Model viewer**
  - Model loader is supported by *Assimp*
  - Texture loader is supported by *DirectXTex*
  - Texture usage follows glTF 2.0 standard. You need to offer the path of the folder which stores the textures. It's not guaranteed that *Assimp* will correctly load the texture path. 
  - Skinned animation is implemented under these conditions:
    - Bone offsets transform a mesh from its local space to bone space.
    - Keyframes specify the transformation to the parent node

    - It's tested that animations of .fbx and .GLB format models have higher possibiblities to be correctly displayed.

- **Amplification shader and mesh shader**
  - These are features supported by GPU with Turing or more advanced architectures.

- **Shader model 6.6**
  - HLSL Dynamic resources feature is used to simplify the root signature.
  
- **Viedo memory management**
  - Implemented via buddy system and segregated free lists
  
- **GPU-driven culling**
   - Frustum culling (instance and meshlet)
   - Normal cone backface culling (meshlet)
   - Hi-Z occlusion culling (instance and meshlet)
   - Instance culling is implemented via compute shader
   - Meshlet culling is implemented via amplification shader
- **CSM**
- **SSAO**
- **TAA**
- **OIT**
   - Implemented via per-pixel linked-list
