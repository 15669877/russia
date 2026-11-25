/* ============================================================
 * gen_vsproj.c —— 生成 Visual Studio 工程文件（.vcxproj）与解决方案（.sln）
 *
 * 【为什么用工具生成】
 * vcxproj 是 XML，手写容易漏字段、漏头文件引用、漏 GUID。
 * 用工具保证 9 个工程结构完全一致，改一次全改。
 *
 * 【用法】
 *   gcc gen_vsproj.c -o gen_vsproj.exe
 *   ./gen_vsproj.exe
 *   （会在当前目录生成 9 个 .vcxproj 和 shoot.sln）
 * ============================================================ */
#include <stdio.h>
#include <string.h>

#define NPROJ 9

static const char *PROJ_NAME[NPROJ] = {
    "shoot",
    "step1_display", "step2_move", "step3_shoot",
    "step4_collide", "step5_wave",
    "selftest", "sim", "preview"
};
static const char *PROJ_SRC[NPROJ] = {
    "shoot.c",
    "step1_display.c", "step2_move.c", "step3_shoot.c",
    "step4_collide.c", "step5_wave.c",
    "selftest.c", "sim.c", "preview.c"
};
static const char *PROJ_GUID[NPROJ] = {
    "{B1C2D3E4-0001-4E5F-8A9B-000000000001}",
    "{B1C2D3E4-0002-4E5F-8A9B-000000000002}",
    "{B1C2D3E4-0003-4E5F-8A9B-000000000003}",
    "{B1C2D3E4-0004-4E5F-8A9B-000000000004}",
    "{B1C2D3E4-0005-4E5F-8A9B-000000000005}",
    "{B1C2D3E4-0006-4E5F-8A9B-000000000006}",
    "{B1C2D3E4-0007-4E5F-8A9B-000000000007}",
    "{B1C2D3E4-0008-4E5F-8A9B-000000000008}",
    "{B1C2D3E4-0009-4E5F-8A9B-000000000009}"
};

/* 每个工程需要包含的头文件 */
static const char *PROJ_HEADERS[NPROJ] = {
    "key.h|render.h|entities.h|game.h",
    "render.h|entities.h",
    "key.h|render.h|entities.h",
    "key.h|render.h|entities.h",
    "key.h|render.h|entities.h",
    "key.h|render.h|entities.h|game.h",
    "entities.h|game.h",
    "entities.h|game.h",
    "key.h|render.h|entities.h|game.h"
};

static void write_vcxproj(int i) {
    char path[64];
    snprintf(path, sizeof path, "%s.vcxproj", PROJ_NAME[i]);
    FILE *f = fopen(path, "w");
    if (!f) { printf("ERROR: cannot write %s\n", path); return; }

    fprintf(f,
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<Project DefaultTargets=\"Build\" ToolsVersion=\"15.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n"
"  <ItemGroup Label=\"ProjectConfigurations\">\n"
"    <ProjectConfiguration Include=\"Debug|x64\">\n"
"      <Configuration>Debug</Configuration><Platform>x64</Platform>\n"
"    </ProjectConfiguration>\n"
"    <ProjectConfiguration Include=\"Release|x64\">\n"
"      <Configuration>Release</Configuration><Platform>x64</Platform>\n"
"    </ProjectConfiguration>\n"
"  </ItemGroup>\n"
"  <PropertyGroup Label=\"Globals\">\n"
"    <VCProjectVersion>15.0</VCProjectVersion>\n"
"    <ProjectGuid>%s</ProjectGuid>\n"
"    <RootNamespace>%s</RootNamespace>\n"
"    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>\n"
"  </PropertyGroup>\n"
"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\n"
"  <PropertyGroup Label=\"Configuration\">\n"
"    <ConfigurationType>Application</ConfigurationType>\n"
"    <PlatformToolset>$(DefaultPlatformToolset)</PlatformToolset>\n"
"    <UseDebugLibraries>true</UseDebugLibraries>\n"
"    <CharacterSet>MultiByte</CharacterSet>\n"
"  </PropertyGroup>\n"
"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\n"
"  <ItemDefinitionGroup>\n"
"    <ClCompile>\n"
"      <PreprocessorDefinitions>_CRT_SECURE_NO_WARNINGS;%%(PreprocessorDefinitions)</PreprocessorDefinitions>\n"
"      <WarningLevel>Level3</WarningLevel>\n"
"      <AdditionalOptions>/utf-8 %%(AdditionalOptions)</AdditionalOptions>\n"
"    </ClCompile>\n"
"    <Link>\n"
"      <SubSystem>Console</SubSystem>\n"
"    </Link>\n"
"  </ItemDefinitionGroup>\n"
"  <ItemGroup>\n"
"    <ClCompile Include=\"%s\" />\n"
"  </ItemGroup>\n"
"  <ItemGroup>\n",
        PROJ_GUID[i], PROJ_NAME[i], PROJ_SRC[i]);

    /* 头文件引用 */
    char list[128];
    snprintf(list, sizeof list, "%s", PROJ_HEADERS[i]);
    char *tok = strtok(list, "|");
    while (tok) {
        fprintf(f, "    <ClInclude Include=\"%s\" />\n", tok);
        tok = strtok(NULL, "|");
    }

    fprintf(f,
"  </ItemGroup>\n"
"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\n"
"</Project>\n");

    fclose(f);
    printf("生成 %s\n", path);
}

static void write_sln(void) {
    FILE *f = fopen("shoot.sln", "w");
    if (!f) { printf("ERROR: cannot write shoot.sln\n"); return; }

    fprintf(f,
"Microsoft Visual Studio Solution File, Format Version 12.00\n"
"# Visual Studio Version 17\n"
"VisualStudioVersion = 17.0.31903.59\n"
"MinimumVisualStudioVersion = 10.0.40219.1\n");

    for (int i = 0; i < NPROJ; i++)
        fprintf(f,
"Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"%s\", \"%s.vcxproj\", \"%s\"\n"
"EndProject\n",
            PROJ_NAME[i], PROJ_NAME[i], PROJ_GUID[i]);

    fprintf(f,
"Global\n"
"\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n"
"\t\tDebug|x64 = Debug|x64\n"
"\t\tRelease|x64 = Release|x64\n"
"\tEndGlobalSection\n"
"\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n");

    for (int i = 0; i < NPROJ; i++) {
        fprintf(f, "\t\t%s.Debug|x64.ActiveCfg = Debug|x64\n", PROJ_GUID[i]);
        fprintf(f, "\t\t%s.Debug|x64.Build.0 = Debug|x64\n", PROJ_GUID[i]);
        fprintf(f, "\t\t%s.Release|x64.ActiveCfg = Release|x64\n", PROJ_GUID[i]);
        fprintf(f, "\t\t%s.Release|x64.Build.0 = Release|x64\n", PROJ_GUID[i]);
    }

    fprintf(f,
"\tEndGlobalSection\n"
"\tGlobalSection(SolutionProperties) = preSolution\n"
"\t\tHideSolutionNode = FALSE\n"
"\tEndGlobalSection\n"
"EndGlobal\n");

    fclose(f);
    printf("生成 shoot.sln\n");
}

int main(void) {
    for (int i = 0; i < NPROJ; i++) write_vcxproj(i);
    write_sln();
    return 0;
}
