# CarolRenderer

## Dependencies:

- *Assimp* 5.2.5
- *DirectXTex* 2022.12.18.1
- *D3D12 Agility* 1.608.2
- *DirectX Shader Compiler* 1.7.2212.23

## Rendering Pipeline

![pipeline](https://i.lensdump.com/i/T7S1XF.png)

## Samples

![THao65.png](https://i3.lensdump.com/i/THao65.png)

![THauzz.png](https://i.lensdump.com/i/THauzz.png)

![THaDwT.png](https://i3.lensdump.com/i/THaDwT.png)

![THaPtb.png](https://i2.lensdump.com/i/THaPtb.png)

## Features:

- **Model viewer**
  - Model loader based on *Assimp*
    - Currently there's difficulties on identifying whether a mesh needs to be rendering with executing alpha blending, so all meshes will be rendered opaque now. Future updates will support this feature.
    - Skinned animation is implemented under these conditions:
      - Bone offsets transform a mesh from its local space to bone space.
      - Keyframes specify the transformation to the parent node
      - It's tested that animations of .fbx and .GLB files have higher possibiblity to be correctly displayed.
  - Texture loader based on *DirectXTex*
    - Texture usage follows glTF 2.0 standard. You need to offer the path of the folder which stores the textures. It's not guaranteed that *Assimp* will correctly load the texture path. 
  

- **Physically Based Shading**
  - These are default and the only supported PBR settings now. GUI for PBR settings is under development.
  - Subsurface Scattering BRDF: Lambertian BRDF
  - Specular BRDF:
    - Normal Distribution Function: GGX Normal Distribution Function
    - Geometry Function: Height-Correlated Masking and Shadowing Function

- **Blinn-Phong Shading**
  - Specular term is augmented by Schlick Fresnel effect and a equation for approximating the normal distribution function (from DX12 dragon book)  

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

- **Cascaded Shadow Map**
  - With split level specified to 5 in default

- **Screen-Space Ambient-Occlusion**
  - Implemented via compute shader with 3 times edge-preserving filtering in default
- **Temporal Anti-Aliasing**
  - Jitter the sample position of a pixel via Halton low-discrepancy sequence (from Unreal Engine 4)
  - Blend current and history pixel colors in YCoCg space (from Unreal Engine 4)
  - Clip the history pixel color via the bounding-box constructed by the expectation and variance of colors of the sampled neighboring colors of the current pixel (from NVIDIA)
- **OIT**
  - Implemented via per-pixel linked-list
