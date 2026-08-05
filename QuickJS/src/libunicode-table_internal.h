#include "libunicode-table.h"


/* 基础数组 */
void fill_case_conv_table1(uint32_t *p);
void fill_case_conv_table2(uint8_t *p);
void fill_case_conv_ext(uint16_t *p);
void fill_unicode_prop_Cased1_table(uint8_t *p);
void fill_unicode_prop_Cased1_index(uint8_t *p);
void fill_unicode_prop_Case_Ignorable_table(uint8_t *p);
void fill_unicode_prop_Case_Ignorable_index(uint8_t *p);
void fill_unicode_prop_ID_Start_table(uint8_t *p);
void fill_unicode_prop_ID_Start_index(uint8_t *p);
void fill_unicode_prop_ID_Continue1_table(uint8_t *p);
void fill_unicode_prop_ID_Continue1_index(uint8_t *p);

/* 条件编译 CONFIG_ALL_UNICODE 下的数组 */
void fill_unicode_cc_table(uint8_t *p);
void fill_unicode_cc_index(uint8_t *p);
void fill_unicode_decomp_table1(uint32_t *p);
void fill_unicode_decomp_table2(uint16_t *p);
void fill_unicode_decomp_data(uint8_t *p);
void fill_unicode_comp_table(uint16_t *p);
void fill_unicode_gc_name_table(char *p);
void fill_unicode_gc_table(uint8_t *p);
void fill_unicode_script_name_table(char *p);
void fill_unicode_script_table(uint8_t *p);
void fill_unicode_script_ext_table(uint8_t *p);

/* 各属性表（均为 uint8_t 数组，但注意有些是 char* 但视为 uint8_t 填充） */
void fill_unicode_prop_Hyphen_table(uint8_t *p);
void fill_unicode_prop_Other_Math_table(uint8_t *p);
void fill_unicode_prop_Other_Alphabetic_table(uint8_t *p);
void fill_unicode_prop_Other_Lowercase_table(uint8_t *p);
void fill_unicode_prop_Other_Uppercase_table(uint8_t *p);
void fill_unicode_prop_Other_Grapheme_Extend_table(uint8_t *p);
void fill_unicode_prop_Other_Default_Ignorable_Code_Point_table(uint8_t *p);
void fill_unicode_prop_Other_ID_Start_table(uint8_t *p);
void fill_unicode_prop_Other_ID_Continue_table(uint8_t *p);
void fill_unicode_prop_Prepended_Concatenation_Mark_table(uint8_t *p);
void fill_unicode_prop_XID_Start1_table(uint8_t *p);
void fill_unicode_prop_XID_Continue1_table(uint8_t *p);
void fill_unicode_prop_Changes_When_Titlecased1_table(uint8_t *p);
void fill_unicode_prop_Changes_When_Casefolded1_table(uint8_t *p);
void fill_unicode_prop_Changes_When_NFKC_Casefolded1_table(uint8_t *p);
void fill_unicode_prop_Basic_Emoji1_table(uint8_t *p);
void fill_unicode_prop_Basic_Emoji2_table(uint8_t *p);
void fill_unicode_prop_RGI_Emoji_Modifier_Sequence_table(uint8_t *p);
void fill_unicode_prop_RGI_Emoji_Flag_Sequence_table(uint8_t *p);
void fill_unicode_prop_Emoji_Keycap_Sequence_table(uint8_t *p);
void fill_unicode_prop_ASCII_Hex_Digit_table(uint8_t *p);
void fill_unicode_prop_Bidi_Control_table(uint8_t *p);
void fill_unicode_prop_Dash_table(uint8_t *p);
void fill_unicode_prop_Deprecated_table(uint8_t *p);
void fill_unicode_prop_Diacritic_table(uint8_t *p);
void fill_unicode_prop_Extender_table(uint8_t *p);
void fill_unicode_prop_Hex_Digit_table(uint8_t *p);
void fill_unicode_prop_IDS_Unary_Operator_table(uint8_t *p);
void fill_unicode_prop_IDS_Binary_Operator_table(uint8_t *p);
void fill_unicode_prop_IDS_Trinary_Operator_table(uint8_t *p);
void fill_unicode_prop_Ideographic_table(uint8_t *p);
void fill_unicode_prop_Join_Control_table(uint8_t *p);
void fill_unicode_prop_Logical_Order_Exception_table(uint8_t *p);
void fill_unicode_prop_Modifier_Combining_Mark_table(uint8_t *p);
void fill_unicode_prop_Noncharacter_Code_Point_table(uint8_t *p);
void fill_unicode_prop_Pattern_Syntax_table(uint8_t *p);
void fill_unicode_prop_Pattern_White_Space_table(uint8_t *p);
void fill_unicode_prop_Quotation_Mark_table(uint8_t *p);
void fill_unicode_prop_Radical_table(uint8_t *p);
void fill_unicode_prop_Regional_Indicator_table(uint8_t *p);
void fill_unicode_prop_Sentence_Terminal_table(uint8_t *p);
void fill_unicode_prop_Soft_Dotted_table(uint8_t *p);
void fill_unicode_prop_Terminal_Punctuation_table(uint8_t *p);
void fill_unicode_prop_Unified_Ideograph_table(uint8_t *p);
void fill_unicode_prop_Variation_Selector_table(uint8_t *p);
void fill_unicode_prop_White_Space_table(uint8_t *p);
void fill_unicode_prop_Bidi_Mirrored_table(uint8_t *p);
void fill_unicode_prop_Emoji_table(uint8_t *p);
void fill_unicode_prop_Emoji_Component_table(uint8_t *p);
void fill_unicode_prop_Emoji_Modifier_table(uint8_t *p);
void fill_unicode_prop_Emoji_Modifier_Base_table(uint8_t *p);
void fill_unicode_prop_Emoji_Presentation_table(uint8_t *p);
void fill_unicode_prop_Extended_Pictographic_table(uint8_t *p);
void fill_unicode_prop_Default_Ignorable_Code_Point_table(uint8_t *p);

/* 指针数组和长度数组（特殊类型） */
void fill_unicode_prop_table(const uint8_t **p);
void fill_unicode_prop_len_table(uint16_t *p);

/* 名称字符串表（char 数组，填充时使用 memcpy 或逐字符赋值，但声明统一为 char*） */
void fill_unicode_prop_name_table(char *p);
void fill_unicode_sequence_prop_name_table(char *p);

/* RGI 序列表（uint8_t 数组） */
void fill_unicode_rgi_emoji_tag_sequence(uint8_t *p);
void fill_unicode_rgi_emoji_zwj_sequence(uint8_t *p);

void fill_unicode_gc_name_table(char *p);
void fill_unicode_script_name_table(char *p);
void fill_unicode_prop_name_table(char *p);
void fill_unicode_sequence_prop_name_table(char *p);