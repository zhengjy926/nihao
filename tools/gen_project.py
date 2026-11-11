#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate mdk/nihao.uvprojx and mdk/scatter.sct from the file list below."""

from __future__ import annotations

import argparse
from pathlib import Path

LEVELS = ("EARLY", "PRE_KERNEL_1", "PRE_KERNEL_2", "POST_KERNEL", "APPLICATION", "SMP")

GROUPS = {
    "startup": [
        ("startup_stm32f429xx.s", 2, r"..\src\startup_stm32f429xx.s"),
    ],
    "system": [
        ("system_stm32f4xx.c", 1,
         r"..\third_party\modules\hal\stm32\stm32cube\stm32f4xx\soc\system_stm32f4xx.c"),
        ("stm32f4xx_ll_dma.c", 1,
         r"..\third_party\modules\hal\stm32\stm32cube\stm32f4xx\drivers\src\stm32f4xx_ll_dma.c"),
        ("stm32f4xx_ll_utils.c", 1,
         r"..\third_party\modules\hal\stm32\stm32cube\stm32f4xx\drivers\src\stm32f4xx_ll_utils.c"),
    ],
    "app": [
        ("main.c", 1, r"..\src\main.c"),
    ],
    "port": [
        ("rodata_bounds.s", 2, r"..\port\src\rodata_bounds.s"),
        ("init_runner.c", 1, r"..\port\src\init_runner.c"),
        ("errno_shim.c", 1, r"..\port\src\errno_shim.c"),
        ("arch_irq_shim.c", 1, r"..\port\src\arch_irq_shim.c"),
        ("device_list_bounds.c", 1, r"..\port\src\device_list_bounds.c"),
        ("log_section_bounds.c", 1, r"..\port\src\log_section_bounds.c"),
        ("irq_loader.c", 1, r"..\port\src\irq_loader.c"),
        ("kernel_shim.c", 1, r"..\port\src\kernel_shim.c"),
        ("libc_shim.c", 1, r"..\port\src\libc_shim.c"),
        ("hal_shim.c", 1, r"..\port\src\hal_shim.c"),
        ("sys_clock_glue.c", 1, r"..\port\src\sys_clock_glue.c"),
    ],
    "zephyr/kernel": [
        ("device.c", 1, r"..\third_party\zephyr\kernel\device.c"),
    ],
    "zephyr/drivers": [
        ("gpio_stm32.c", 1, r"..\third_party\zephyr\drivers\gpio\gpio_stm32.c"),
        ("pinctrl_stm32.c", 1, r"..\third_party\zephyr\drivers\pinctrl\pinctrl_stm32.c"),
        ("clock_stm32f2_f4_f7.c", 1,
         r"..\third_party\zephyr\drivers\clock_control\clock_stm32f2_f4_f7.c"),
        ("clock_stm32_ll_common.c", 1,
         r"..\third_party\zephyr\drivers\clock_control\clock_stm32_ll_common.c"),
        ("intc_gpio_stm32.c", 1,
         r"..\third_party\zephyr\drivers\interrupt_controller\intc_gpio_stm32.c"),
        ("intc_exti_stm32.c", 1,
         r"..\third_party\zephyr\drivers\interrupt_controller\intc_exti_stm32.c"),
        ("uart_stm32.c", 1,
         r"..\third_party\zephyr\drivers\serial\uart_stm32.c"),
        ("uart_console.c", 1,
         r"..\third_party\zephyr\drivers\console\uart_console.c"),
        ("reset_stm32.c", 1,
         r"..\third_party\zephyr\drivers\reset\reset_stm32.c"),
        ("pinctrl_common.c", 1,
         r"..\third_party\zephyr\drivers\pinctrl\common.c"),
        ("dma_stm32.c", 1,
         r"..\third_party\zephyr\drivers\dma\dma_stm32.c"),
        ("dma_stm32_v1.c", 1,
         r"..\third_party\zephyr\drivers\dma\dma_stm32_v1.c"),
        ("cortex_m_systick.c", 1,
         r"..\third_party\zephyr\drivers\timer\cortex_m_systick.c"),
    ],
    "zephyr/subsys/logging": [
        ("log_core.c", 1, r"..\third_party\zephyr\subsys\logging\log_core.c"),
        ("log_msg.c", 1, r"..\third_party\zephyr\subsys\logging\log_msg.c"),
        ("log_output.c", 1, r"..\third_party\zephyr\subsys\logging\log_output.c"),
        ("log_cache.c", 1, r"..\third_party\zephyr\subsys\logging\log_cache.c"),
        ("log_mgmt.c", 1, r"..\third_party\zephyr\subsys\logging\log_mgmt.c"),
        ("log_backend_uart.c", 1,
         r"..\third_party\zephyr\subsys\logging\backends\log_backend_uart.c"),
    ],
    "zephyr/lib/os": [
        ("cbprintf.c", 1, r"..\third_party\zephyr\lib\os\cbprintf.c"),
        ("cbprintf_complete.c", 1, r"..\third_party\zephyr\lib\os\cbprintf_complete.c"),
        ("cbprintf_packaged.c", 1, r"..\third_party\zephyr\lib\os\cbprintf_packaged.c"),
        ("printk.c", 1, r"..\third_party\zephyr\lib\os\printk.c"),
        ("mpsc_pbuf.c", 1, r"..\third_party\zephyr\lib\os\mpsc_pbuf.c"),
    ],
    "zephyr/soc": [
        ("gpioport_mgr.c", 1,
         r"..\third_party\zephyr\soc\st\stm32\common\gpioport_mgr.c"),
    ],
    "zephyr/libc": [
        ("errno.c", 1, r"..\third_party\zephyr\lib\libc\armstdc\src\errno.c"),
    ],
}

INCLUDES = [
    r"..\port\include",
    r"..\config_gen\4.5.0",
    r"..\third_party\zephyr\include",
    r"..\third_party\zephyr\arch\common\include",
    r"..\third_party\zephyr\soc\st\stm32\stm32f4x",
    r"..\third_party\zephyr\soc\st\stm32\common",
    r"..\third_party\zephyr\modules\cmsis_6",
    r"..\third_party\zephyr\lib\libc\armstdc\include",
    r"..\third_party\zephyr\lib\libc\common\include",
    r"..\third_party\modules\hal\cmsis_6\CMSIS\Core\Include",
    r"..\third_party\modules\hal\stm32\stm32cube\stm32f4xx\soc",
    r"..\third_party\modules\hal\stm32\stm32cube\stm32f4xx\drivers\include",
    r"..\third_party\modules\hal\stm32\stm32cube\common_ll\include",
]

DEFINES = "__ZEPHYR__=1,CORE_CM4,STM32F429xx,USE_FULL_LL_DRIVER,USE_HAL_DRIVER,HSE_VALUE=25000000"
MISC = "-imacros ../config_gen/4.5.0/zephyr/autoconf.h"

SCATTER_HEADER = """\
; Generated by tools/gen_project.py - do not edit.
;
; Note: armlink only honours explicit section selectors in regions without a
; fixed base address. Every iterable list therefore gets its own "+0" region.
LR_IROM1 0x08000000 0x00100000
{
  ER_IROM1 0x08000000 0x00100000
  {
    *(RESET, +First)
    *(InRoot$$Sections)
    .ANY (+RO)
  }
"""

SCATTER_TAIL = """
  ER_INTLIST +0
  {
    *(.nihao_intlist_begin, +First)
    *(.intList)
  }

  ER_INTLIST_END +0
  {
    *(.nihao_intlist_end)
  }

  ER_LOG_CONST +0
  {
    *(.nihao_log_const_begin, +First)
    *(._log_const.static.*)
    *(.nihao_log_const_end, +Last)
  }

  ER_LOG_BACKEND +0
  {
    *(.nihao_log_backend_begin, +First)
    *(._log_backend.static.*)
    *(.nihao_log_backend_end, +Last)
  }

  ER_DEVICE +0
  {
    *(.nihao_device_begin, +First)
    *(._device.static.*)
    *(.nihao_device_end, +Last)
  }

  RW_IRAM1 0x20000000 0x0002DE00
  {
    .ANY (+RW +ZI)
  }

  ER_LOG_MSG_PTR +0
  {
    *(.nihao_log_msg_ptr_begin, +First)
    *(._log_msg_ptr.static.*)
  }

  ER_LOG_MSG_PTR_END +0
  {
    *(.nihao_log_msg_ptr_end)
  }

  ER_LOG_DYNAMIC +0
  {
    *(.nihao_log_dynamic_begin, +First)
    *(._log_dynamic.static.*)
  }

  ER_LOG_DYNAMIC_END +0
  {
    *(.nihao_log_dynamic_end)
  }

  ER_LOG_LINK +0
  {
    *(.nihao_log_link_begin, +First)
    *(._log_link.static.*)
  }

  ER_LOG_LINK_END +0
  {
    *(.nihao_log_link_end)
  }

  ER_LOG_MPSC +0
  {
    *(.nihao_log_mpsc_pbuf_begin, +First)
    *(._log_mpsc_pbuf.static.*)
  }

  ER_LOG_MPSC_END +0
  {
    *(.nihao_log_mpsc_pbuf_end)
  }

  ER_HEAP 0x2002DE00 UNINIT 0x00000200
  {
    *(HEAP)
  }

  ER_STACK 0x2002E000 UNINIT 0x00002000
  {
    *(STACK)
  }
}
"""


def scatter_text() -> str:
    parts = [SCATTER_HEADER]
    for level in LEVELS:
        parts.append(
            f"  ER_INIT_{level} +0\n"
            "  {\n"
            f"    *(.aaa_{level}_begin)\n"
            f"    *(.z_init_{level}_P_*)\n"
            f"    *(.zzz_{level}_end)\n"
            "  }\n"
        )
    parts.append(SCATTER_TAIL)
    return "".join(parts)


def groups_xml() -> str:
    out = []
    for name, files in GROUPS.items():
        entries = []
        for fname, ftype, fpath in files:
            entries.append(
                "        <File>\n"
                f"          <FileName>{fname}</FileName>\n"
                f"          <FileType>{ftype}</FileType>\n"
                f"          <FilePath>{fpath}</FilePath>\n"
                "        </File>"
            )
        out.append(
            "      <Group>\n"
            f"        <GroupName>{name}</GroupName>\n"
            "        <Files>\n"
            + "\n".join(entries)
            + "\n        </Files>\n      </Group>"
        )
    return "\n".join(out)


def project_text(include_path: str, groups: str) -> str:
    return f"""<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<Project xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="project_projx.xsd">

  <SchemaVersion>2.1</SchemaVersion>

  <Header>### uVision Project, (C) Keil Software</Header>

  <Targets>
    <Target>
      <TargetName>nihao</TargetName>
      <ToolsetNumber>0x4</ToolsetNumber>
      <ToolsetName>ARM-ADS</ToolsetName>
      <pCCUsed>6230000::V6.23::ARMCLANG</pCCUsed>
      <uAC6>1</uAC6>
      <TargetOption>
        <TargetCommonOption>
          <Device>STM32F429IGTx</Device>
          <Vendor>STMicroelectronics</Vendor>
          <PackID>Keil.STM32F4xx_DFP.3.1.0</PackID>
          <PackURL>https://www.keil.com/pack/</PackURL>
          <Cpu>IRAM(0x20000000,0x00030000) IROM(0x08000000,0x00100000) CPUTYPE("Cortex-M4") FPU2 CLOCK(25000000) ELITTLE</Cpu>
          <FlashUtilSpec></FlashUtilSpec>
          <StartupFile></StartupFile>
          <FlashDriverDll>UL2CM3(-S0 -C0 -P0 -FD20000000 -FC1000 -FN1 -FF0STM32F4xx_1024 -FS08000000 -FL0100000 -FP0($$Device:STM32F429IGTx$CMSIS\\Flash\\STM32F4xx_1024.FLM))</FlashDriverDll>
          <DeviceId>0</DeviceId>
          <RegisterFile></RegisterFile>
          <MemoryEnv></MemoryEnv>
          <Cmp></Cmp>
          <Asm></Asm>
          <Linker></Linker>
          <OHString></OHString>
          <InfinionOptionDll></InfinionOptionDll>
          <SLE66CMisc></SLE66CMisc>
          <SLE66AMisc></SLE66AMisc>
          <SLE66LinkerMisc></SLE66LinkerMisc>
          <SFDFile>$$Device:STM32F429IGTx$CMSIS\\SVD\\STM32F429x.svd</SFDFile>
          <bCustSvd>0</bCustSvd>
          <UseEnv>0</UseEnv>
          <BinPath></BinPath>
          <IncludePath></IncludePath>
          <LibPath></LibPath>
          <RegisterFilePath></RegisterFilePath>
          <DBRegisterFilePath></DBRegisterFilePath>
          <TargetStatus>
            <Error>0</Error>
            <ExitCodeStop>0</ExitCodeStop>
            <ButtonStop>0</ButtonStop>
            <NotGenerated>0</NotGenerated>
            <InvalidFlash>1</InvalidFlash>
          </TargetStatus>
          <OutputDirectory>.\\Objects\\</OutputDirectory>
          <OutputName>nihao</OutputName>
          <CreateExecutable>1</CreateExecutable>
          <CreateLib>0</CreateLib>
          <CreateHexFile>1</CreateHexFile>
          <DebugInformation>1</DebugInformation>
          <BrowseInformation>1</BrowseInformation>
          <ListingPath>.\\Listings\\</ListingPath>
          <HexFormatSelection>1</HexFormatSelection>
          <Merge32K>0</Merge32K>
          <CreateBatchFile>0</CreateBatchFile>
          <BeforeCompile>
            <RunUserProg1>0</RunUserProg1>
            <RunUserProg2>0</RunUserProg2>
            <UserProg1Name></UserProg1Name>
            <UserProg2Name></UserProg2Name>
            <UserProg1Dos16Mode>0</UserProg1Dos16Mode>
            <UserProg2Dos16Mode>0</UserProg2Dos16Mode>
            <nStopU1X>0</nStopU1X>
            <nStopU2X>0</nStopU2X>
          </BeforeCompile>
          <BeforeMake>
            <RunUserProg1>0</RunUserProg1>
            <RunUserProg2>0</RunUserProg2>
            <UserProg1Name></UserProg1Name>
            <UserProg2Name></UserProg2Name>
            <UserProg1Dos16Mode>0</UserProg1Dos16Mode>
            <UserProg2Dos16Mode>0</UserProg2Dos16Mode>
            <nStopB1X>0</nStopB1X>
            <nStopB2X>0</nStopB2X>
          </BeforeMake>
          <AfterMake>
            <RunUserProg1>0</RunUserProg1>
            <RunUserProg2>0</RunUserProg2>
            <UserProg1Name></UserProg1Name>
            <UserProg2Name></UserProg2Name>
            <UserProg1Dos16Mode>0</UserProg1Dos16Mode>
            <UserProg2Dos16Mode>0</UserProg2Dos16Mode>
          </AfterMake>
          <SelectedEnvironment>External ARMC6</SelectedEnvironment>
          <VariousControls>
            <MiscControls></MiscControls>
            <Define></Define>
            <Undefine></Undefine>
            <IncludePath></IncludePath>
          </VariousControls>
        </TargetCommonOption>
        <CommonProperty>
          <UseCPPCompiler>0</UseCPPCompiler>
          <RVCTCodeConst>0</RVCTCodeConst>
          <RVCTZI>0</RVCTZI>
          <RVCTOtherData>0</RVCTOtherData>
          <ModuleSelection>0</ModuleSelection>
          <IncludeInBuild>1</IncludeInBuild>
          <AlwaysBuild>0</AlwaysBuild>
          <GenerateAssemblyFile>0</GenerateAssemblyFile>
          <AssembleAssemblyFile>0</AssembleAssemblyFile>
          <PublicsOnly>0</PublicsOnly>
          <StopOnExitCode>3</StopOnExitCode>
          <CustomArgument></CustomArgument>
          <IncludeLibraryModules></IncludeLibraryModules>
          <ComprImg>0</ComprImg>
        </CommonProperty>
        <DllOption>
          <SimDllName>SARMCM3.DLL</SimDllName>
          <SimDllArguments> -REMAP -MPU</SimDllArguments>
          <SimDlgDll>DCM.DLL</SimDlgDll>
          <SimDlgDllArguments>-pCM4</SimDlgDllArguments>
          <TargetDllName>SARMCM3.DLL</TargetDllName>
          <TargetDllArguments> -MPU</TargetDllArguments>
          <TargetDlgDll>TCM.DLL</TargetDlgDll>
          <TargetDlgDllArguments>-pCM4</TargetDlgDllArguments>
        </DllOption>
        <DebugOption>
          <OPTHX>
            <HexSelection>1</HexSelection>
            <HexRangeLowAddress>0</HexRangeLowAddress>
            <HexRangeHighAddress>0</HexRangeHighAddress>
            <HexOffset>0</HexOffset>
            <Oh166RecLen>16</Oh166RecLen>
          </OPTHX>
        </DebugOption>
        <Utilities>
          <Flash1>
            <UseTargetDll>1</UseTargetDll>
            <UseExternalTool>0</UseExternalTool>
            <RunIndependent>0</RunIndependent>
            <UpdateFlashBeforeDebugging>1</UpdateFlashBeforeDebugging>
            <Capability>1</Capability>
            <DriverSelection>4096</DriverSelection>
          </Flash1>
          <bUseTDR>1</bUseTDR>
          <Flash2>BIN\\UL2CM3.DLL</Flash2>
          <Flash3>"" ()</Flash3>
          <Flash4></Flash4>
          <pFcarmOut></pFcarmOut>
          <pFcarmGrp></pFcarmGrp>
          <pFcArmRoot></pFcArmRoot>
          <FcArmLst>0</FcArmLst>
        </Utilities>
        <TargetArmAds>
          <ArmAdsMisc>
            <GenerateListings>0</GenerateListings>
            <asHll>1</asHll>
            <asAsm>1</asAsm>
            <asMacX>1</asMacX>
            <asSyms>1</asSyms>
            <asFals>1</asFals>
            <asDbgD>1</asDbgD>
            <asForm>1</asForm>
            <ldLst>0</ldLst>
            <ldmm>1</ldmm>
            <ldXref>1</ldXref>
            <BigEnd>0</BigEnd>
            <AdsALst>1</AdsALst>
            <AdsACrf>1</AdsACrf>
            <AdsANop>0</AdsANop>
            <AdsANot>0</AdsANot>
            <AdsLLst>1</AdsLLst>
            <AdsLmap>1</AdsLmap>
            <AdsLcgr>1</AdsLcgr>
            <AdsLsym>1</AdsLsym>
            <AdsLszi>1</AdsLszi>
            <AdsLtoi>1</AdsLtoi>
            <AdsLsun>1</AdsLsun>
            <AdsLven>1</AdsLven>
            <AdsLsxf>1</AdsLsxf>
            <RvctClst>0</RvctClst>
            <GenPPlst>0</GenPPlst>
            <AdsCpuType>"Cortex-M4"</AdsCpuType>
            <RvctDeviceName></RvctDeviceName>
            <mOS>0</mOS>
            <uocRom>0</uocRom>
            <uocRam>0</uocRam>
            <hadIROM>1</hadIROM>
            <hadIRAM>1</hadIRAM>
            <hadXRAM>0</hadXRAM>
            <uocXRam>0</uocXRam>
            <RvdsVP>2</RvdsVP>
            <RvdsMve>0</RvdsMve>
            <RvdsCdeCp>0</RvdsCdeCp>
            <nBranchProt>0</nBranchProt>
            <hadIRAM2>0</hadIRAM2>
            <hadIROM2>0</hadIROM2>
            <StupSel>8</StupSel>
            <useUlib>0</useUlib>
            <EndSel>0</EndSel>
            <uLtcg>0</uLtcg>
            <nSecure>0</nSecure>
            <RoSelD>3</RoSelD>
            <RwSelD>3</RwSelD>
            <CodeSel>0</CodeSel>
            <OptFeed>0</OptFeed>
            <NoZi1>0</NoZi1>
            <NoZi2>0</NoZi2>
            <NoZi3>0</NoZi3>
            <NoZi4>0</NoZi4>
            <NoZi5>0</NoZi5>
            <Ro1Chk>0</Ro1Chk>
            <Ro2Chk>0</Ro2Chk>
            <Ro3Chk>0</Ro3Chk>
            <Ir1Chk>1</Ir1Chk>
            <Ir2Chk>0</Ir2Chk>
            <Ra1Chk>0</Ra1Chk>
            <Ra2Chk>0</Ra2Chk>
            <Ra3Chk>0</Ra3Chk>
            <Im1Chk>1</Im1Chk>
            <Im2Chk>0</Im2Chk>
            <OnChipMemories>
              <Ocm1><Type>0</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></Ocm1>
              <Ocm2><Type>0</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></Ocm2>
              <Ocm3><Type>0</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></Ocm3>
              <Ocm4><Type>0</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></Ocm4>
              <Ocm5><Type>0</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></Ocm5>
              <Ocm6><Type>0</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></Ocm6>
              <IRAM><Type>0</Type><StartAddress>0x20000000</StartAddress><Size>0x30000</Size></IRAM>
              <IROM><Type>0</Type><StartAddress>0x8000000</StartAddress><Size>0x100000</Size></IROM>
              <XRAM><Type>0</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></XRAM>
              <OCR_RVCT1><Type>1</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></OCR_RVCT1>
              <OCR_RVCT2><Type>1</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></OCR_RVCT2>
              <OCR_RVCT3><Type>1</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></OCR_RVCT3>
              <OCR_RVCT4><Type>1</Type><StartAddress>0x8000000</StartAddress><Size>0x100000</Size></OCR_RVCT4>
              <OCR_RVCT5><Type>1</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></OCR_RVCT5>
              <OCR_RVCT6><Type>0</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></OCR_RVCT6>
              <OCR_RVCT7><Type>0</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></OCR_RVCT7>
              <OCR_RVCT8><Type>0</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></OCR_RVCT8>
              <OCR_RVCT9><Type>0</Type><StartAddress>0x20000000</StartAddress><Size>0x30000</Size></OCR_RVCT9>
              <OCR_RVCT10><Type>0</Type><StartAddress>0x0</StartAddress><Size>0x0</Size></OCR_RVCT10>
            </OnChipMemories>
            <RvctStartVector></RvctStartVector>
          </ArmAdsMisc>
          <Cads>
            <interw>1</interw>
            <Optim>2</Optim>
            <oTime>0</oTime>
            <SplitLS>0</SplitLS>
            <OneElfS>1</OneElfS>
            <Strict>0</Strict>
            <EnumInt>0</EnumInt>
            <PlainCh>0</PlainCh>
            <Ropi>0</Ropi>
            <Rwpi>0</Rwpi>
            <wLevel>1</wLevel>
            <uThumb>1</uThumb>
            <uSurpInc>0</uSurpInc>
            <uC99>1</uC99>
            <uGnu>1</uGnu>
            <useXO>0</useXO>
            <v6Lang>3</v6Lang>
            <v6LangP>3</v6LangP>
            <vShortEn>0</vShortEn>
            <vShortWch>0</vShortWch>
            <v6Lto>0</v6Lto>
            <v6WtE>0</v6WtE>
            <v6Rtti>0</v6Rtti>
            <VariousControls>
              <MiscControls>{MISC}</MiscControls>
              <Define>{DEFINES}</Define>
              <Undefine></Undefine>
              <IncludePath>{include_path}</IncludePath>
            </VariousControls>
          </Cads>
          <Aads>
            <interw>1</interw>
            <Ropi>0</Ropi>
            <Rwpi>0</Rwpi>
            <thumb>1</thumb>
            <SplitLS>0</SplitLS>
            <SwStkChk>0</SwStkChk>
            <NoWarn>0</NoWarn>
            <uSurpInc>0</uSurpInc>
            <useXO>0</useXO>
            <ClangAsOpt>4</ClangAsOpt>
            <VariousControls>
              <MiscControls></MiscControls>
              <Define></Define>
              <Undefine></Undefine>
              <IncludePath></IncludePath>
            </VariousControls>
          </Aads>
          <LDads>
            <umfTarg>0</umfTarg>
            <Ropi>0</Ropi>
            <Rwpi>0</Rwpi>
            <noStLib>0</noStLib>
            <RepFail>1</RepFail>
            <useFile>1</useFile>
            <TextAddressRange>0x08000000</TextAddressRange>
            <DataAddressRange>0x20000000</DataAddressRange>
            <pXoBase></pXoBase>
            <ScatterFile>.\\scatter.sct</ScatterFile>
            <IncludeLibs></IncludeLibs>
            <IncludeLibsPath></IncludeLibsPath>
            <Misc>--entry Reset_Handler --remove --diag_suppress L6314W</Misc>
            <LinkerInputFile></LinkerInputFile>
            <DisabledWarnings></DisabledWarnings>
          </LDads>
        </TargetArmAds>
      </TargetOption>
      <Groups>
{groups}
      </Groups>
    </Target>
  </Targets>
</Project>
"""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path,
                        default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()

    mdk = args.repo / "mdk"
    mdk.mkdir(parents=True, exist_ok=True)

    scatter = mdk / "scatter.sct"
    scatter.write_text(scatter_text(), encoding="ascii", newline="\r\n")

    project = mdk / "nihao.uvprojx"
    project.write_text(project_text(";".join(INCLUDES), groups_xml()),
                       encoding="utf-8", newline="\r\n")

    print(f"Generated {project} and {scatter}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
