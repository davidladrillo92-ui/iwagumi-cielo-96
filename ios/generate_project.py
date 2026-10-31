#!/usr/bin/env python3
"""Deterministic Xcode project, no package manager or third-party build generator."""
from pathlib import Path
import json
root=Path(__file__).resolve().parent
objects={};counter=0
def obj(kind,**values):
    global counter
    counter+=1;key=f'{counter:024X}';objects[key]={'isa':kind,**values};return key
def configs(target=False,uitest=False):
    values={'SDKROOT':'iphoneos','IPHONEOS_DEPLOYMENT_TARGET':'17.0','SWIFT_VERSION':'5.0','CLANG_ENABLE_MODULES':'YES'}
    if target:values.update({'PRODUCT_BUNDLE_IDENTIFIER':'es.iwagumi.cielo','PRODUCT_NAME':'CIELO','INFOPLIST_FILE':'Cielo/Info.plist','GENERATE_INFOPLIST_FILE':'NO','MARKETING_VERSION':'0.2.0','CURRENT_PROJECT_VERSION':'2','TARGETED_DEVICE_FAMILY':'1,2','CODE_SIGN_STYLE':'Automatic','ASSETCATALOG_COMPILER_APPICON_NAME':'AppIcon','SWIFT_EMIT_LOC_STRINGS':'YES'})
    if uitest:values.update({'PRODUCT_BUNDLE_IDENTIFIER':'es.iwagumi.cielo.uitests','PRODUCT_NAME':'CieloUITests','GENERATE_INFOPLIST_FILE':'YES','TEST_TARGET_NAME':'CIELO','CODE_SIGN_STYLE':'Automatic','TARGETED_DEVICE_FAMILY':'1,2'})
    keys=[]
    for name in ['Debug','Release']:
        v=dict(values);v['SWIFT_OPTIMIZATION_LEVEL']='-Onone' if name=='Debug' else '-O'
        keys.append(obj('XCBuildConfiguration',name=name,buildSettings=v))
    return obj('XCConfigurationList',buildConfigurations=keys,defaultConfigurationIsVisible=0,defaultConfigurationName='Release')
product=obj('PBXFileReference',explicitFileType='wrapper.application',path='CIELO.app',sourceTree='BUILT_PRODUCTS_DIR')
files=[];sources=[];resources=[]
for name in ['Core.swift','Controller.swift','Views.swift','CieloApp.swift','Assets.xcassets','PrivacyInfo.xcprivacy','DemoProfile.json','Info.plist']:
    kind='sourcecode.swift' if name.endswith('.swift') else 'folder.assetcatalog' if name.endswith('.xcassets') else 'text.json' if name.endswith('.json') else 'text.plist.xml'
    ref=obj('PBXFileReference',lastKnownFileType=kind,path=name,sourceTree='<group>');files.append(ref)
    if name!='Info.plist':
        build=obj('PBXBuildFile',fileRef=ref)
        (sources if name.endswith('.swift') else resources).append(build)
group=obj('PBXGroup',children=files,path='Cielo',sourceTree='<group>')
products=obj('PBXGroup',children=[product],name='Products',sourceTree='<group>')
main=obj('PBXGroup',children=[group,products],sourceTree='<group>')
phases=[obj('PBXSourcesBuildPhase',buildActionMask=2147483647,files=sources,runOnlyForDeploymentPostprocessing=0),obj('PBXFrameworksBuildPhase',buildActionMask=2147483647,files=[],runOnlyForDeploymentPostprocessing=0),obj('PBXResourcesBuildPhase',buildActionMask=2147483647,files=resources,runOnlyForDeploymentPostprocessing=0)]
target=obj('PBXNativeTarget',buildConfigurationList=configs(True),buildPhases=phases,buildRules=[],dependencies=[],name='CIELO',productName='CIELO',productReference=product,productType='com.apple.product-type.application')
ui_product=obj('PBXFileReference',explicitFileType='wrapper.cfbundle',path='CieloUITests.xctest',sourceTree='BUILT_PRODUCTS_DIR')
ui_file=obj('PBXFileReference',lastKnownFileType='sourcecode.swift',path='CieloUITests.swift',sourceTree='<group>')
ui_group=obj('PBXGroup',children=[ui_file],path='Tests',sourceTree='<group>')
objects[main]['children'].insert(1,ui_group);objects[products]['children'].append(ui_product)
ui_build=obj('PBXBuildFile',fileRef=ui_file)
ui_phases=[obj('PBXSourcesBuildPhase',buildActionMask=2147483647,files=[ui_build],runOnlyForDeploymentPostprocessing=0),obj('PBXFrameworksBuildPhase',buildActionMask=2147483647,files=[],runOnlyForDeploymentPostprocessing=0),obj('PBXResourcesBuildPhase',buildActionMask=2147483647,files=[],runOnlyForDeploymentPostprocessing=0)]
proxy=obj('PBXContainerItemProxy',containerPortal='',proxyType=1,remoteGlobalIDString=target,remoteInfo='CIELO')
dependency=obj('PBXTargetDependency',target=target,targetProxy=proxy)
ui_target=obj('PBXNativeTarget',buildConfigurationList=configs(uitest=True),buildPhases=ui_phases,buildRules=[],dependencies=[dependency],name='CieloUITests',productName='CieloUITests',productReference=ui_product,productType='com.apple.product-type.bundle.ui-testing')
project=obj('PBXProject',attributes={'BuildIndependentTargetsInParallel':'YES','LastUpgradeCheck':'1600'},buildConfigurationList=configs(),compatibilityVersion='Xcode 14.0',developmentRegion='es',hasScannedForEncodings=0,knownRegions=['es','en','Base'],mainGroup=main,productRefGroup=products,projectDirPath='',projectRoot='',targets=[target,ui_target])
objects[proxy]['containerPortal']=project
def encode(value):
    if isinstance(value,dict):return '{\n'+'\n'.join(json.dumps(k)+' = '+encode(v)+';' for k,v in value.items())+'\n}'
    if isinstance(value,list):return '('+','.join(encode(v) for v in value)+')'
    if isinstance(value,int):return str(value)
    return json.dumps(value)
out=root/'Cielo.xcodeproj';out.mkdir(exist_ok=True)
(out/'project.pbxproj').write_text('// !$*UTF8*$!\n'+encode({'archiveVersion':1,'classes':{},'objectVersion':56,'objects':objects,'rootObject':project})+'\n')
schemes=out/'xcshareddata/xcschemes';schemes.mkdir(parents=True,exist_ok=True)
(schemes/'CIELO.xcscheme').write_text(f'''<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion="1600" version="1.3"><BuildAction parallelizeBuildables="YES" buildImplicitDependencies="YES"><BuildActionEntries><BuildActionEntry buildForTesting="YES" buildForRunning="YES" buildForProfiling="YES" buildForArchiving="YES" buildForAnalyzing="YES"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="CIELO.app" BlueprintName="CIELO" ReferencedContainer="container:Cielo.xcodeproj"/></BuildActionEntry><BuildActionEntry buildForTesting="YES" buildForRunning="NO" buildForProfiling="NO" buildForArchiving="NO" buildForAnalyzing="NO"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{ui_target}" BuildableName="CieloUITests.xctest" BlueprintName="CieloUITests" ReferencedContainer="container:Cielo.xcodeproj"/></BuildActionEntry></BuildActionEntries></BuildAction><TestAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" shouldUseLaunchSchemeArgsEnv="YES"><Testables><TestableReference skipped="NO" parallelizable="NO"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{ui_target}" BuildableName="CieloUITests.xctest" BlueprintName="CieloUITests" ReferencedContainer="container:Cielo.xcodeproj"/></TestableReference></Testables></TestAction><LaunchAction buildConfiguration="Debug" selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier="Xcode.IDEFoundation.Launcher.LLDB" launchStyle="0" useCustomWorkingDirectory="NO" ignoresPersistentStateOnLaunch="NO" debugDocumentVersioning="YES" debugServiceExtension="internal" allowLocationSimulation="YES"><BuildableProductRunnable runnableDebuggingMode="0"><BuildableReference BuildableIdentifier="primary" BlueprintIdentifier="{target}" BuildableName="CIELO.app" BlueprintName="CIELO" ReferencedContainer="container:Cielo.xcodeproj"/></BuildableProductRunnable></LaunchAction><ArchiveAction buildConfiguration="Release" revealArchiveInOrganizer="YES"/></Scheme>''')
print('Generated Cielo.xcodeproj')
