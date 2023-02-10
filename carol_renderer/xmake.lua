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
        add_cxxflags("/utf-8","/EHsc","/wd4005","/wd5106","/MP","/translateInclude")
        if is_mode("debug") then
            add_cxxflags("/MDd")
        else
            add_cxxflags("/MD","/O2")
        end
    end


    add_includedirs(
        "src",
        "packages/assimp/include",
        "packages/D3D12 Agility/include",
        "packages/DirectX Shader Compiler/include",
        "packages/DirectXTex/include"
    )

    
    add_linkdirs(
        "packages/assimp/lib/$(mode)",
        "packages/DirectX Shader Compiler/lib/$(arch)",
        "packages/DirectXTex/lib/$(arch)/$(mode)")
    add_links(
        "d3d12",
        "dxgi",
        "dxguid",
        "onecore",
        "dxcompiler",
        "DirectXTex")

    after_build(function (target) 
            os.cp("packages/*/bin/*.dll","build/$(os)/$(arch)/$(mode)")
            os.cp("packages/*/bin/$(mode)/*.dll","build/$(os)/$(arch)/$(mode)")
            os.cp("packages/*/bin/$(arch)/*.dll","build/$(os)/$(arch)/$(mode)")
            os.cp("packages/*/bin/$(arch)/$(mode)/*.dll","build/$(os)/$(arch)/$(mode)")
        end)

    if is_mode("debug") then
        add_links("assimp-vc143-mtd")
        after_build( function (target) 
            os.cp("packages/*/bin/debug/*.pdb","build/$(os)/$(arch)/debug")
            os.cp("packages/*/bin/$(arch)/debug/*.pdb","build/$(os)/$(arch)/debug")
        end)
    elseif is_mode("release") then
        add_links("assimp-vc143-mt")
    end