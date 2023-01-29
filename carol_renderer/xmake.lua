add_rules("mode.debug", "mode.release")

target("carol_renderer")
    set_kind("binary")
    set_languages("c++20")
    set_rundir(".")
    
    add_headerfiles("src/**.h")
    add_files(
        "src/**.ixx",
        "src/**.cpp",
        "src/**.rc")
    
    if is_config("toolchain","msvc") then
        add_cxxflags("/utf-8","/EHsc","/W0","/MDd","/MP","/translateInclude")
    end

    add_includedirs(
        "src",
        "packages/assimp/include",
        "packages/D3D12 Agility/include",
        "packages/DirectX Shader Compiler/include",
        "packages/DirectXTex/include"
    )
    add_linkdirs(
        "packages/assimp/lib",
        "packages/DirectX Shader Compiler/lib",
        "packages/DirectXTex/lib"
    )
    add_links(
        "d3d12",
        "dxgi",
        "dxguid",
        "onecore",
        "assimp-vc143-mtd",
        "dxcompiler",
        "DirectXTex")

    if is_mode("debug") then
        after_build("windows|x64", function (target) 
            os.cp("packages/*/bin/*.dll","build/windows/x64/debug")
            os.cp("packages/*/bin/*.pdb","build/windows/x64/debug")
        end)
    elseif is_mode("release") then
        after_build("windows|x64", function (target) 
            os.cp("packages/*/bin/*.dll","build/windows/x64/release")
            os.cp("packages/*/bin/*.pdb","build/windows/x64/release")
        end)
    end

    
    
