.global target_xml
.global target_xml_len
.global arm_core_xml
.global arm_core_xml_len
.global arm_vfp_xml
.global arm_vfp_xml_len

target_xml:
.incbin "../source/target.xml"
target_xml_end:

arm_core_xml:
.incbin "../source/arm-core.xml"
arm_core_xml_end:

arm_vfp_xml:
.incbin "../source/arm-vfpv2.xml"
arm_vfp_xml_end:

target_xml_len:
.word target_xml_end - target_xml
arm_core_xml_len:
.word arm_core_xml_end - arm_core_xml
arm_vfp_xml_len:
.word arm_vfp_xml_end - arm_vfp_xml