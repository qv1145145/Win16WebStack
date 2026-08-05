/*
 * libunicode-table.c - 动态分配的 Unicode 表
 * 由 gen_unicode_tables 从 libunicode-table.gen 生成
 */

#include "libunicode-table.h"
#include "libunicode-table_internal.h"
#include "ft_malloc.h"
#include <string.h>

uint32_t *case_conv_table1;
uint8_t  *case_conv_table2;
uint16_t *case_conv_ext;
uint8_t *unicode_prop_Cased1_table;
uint8_t *unicode_prop_Cased1_index;
uint8_t *unicode_prop_Case_Ignorable_table;
uint8_t *unicode_prop_Case_Ignorable_index;
uint8_t *unicode_prop_ID_Start_table;
uint8_t *unicode_prop_ID_Start_index;
uint8_t *unicode_prop_ID_Continue1_table;
uint8_t *unicode_prop_ID_Continue1_index;
uint8_t *unicode_cc_table;
uint8_t *unicode_cc_index;
uint32_t *unicode_decomp_table1;
uint16_t *unicode_decomp_table2;
uint8_t *unicode_decomp_data;
uint16_t *unicode_comp_table;
uint8_t *unicode_gc_table;
uint8_t *unicode_script_table;
uint8_t *unicode_script_ext_table;
uint8_t *unicode_prop_Hyphen_table;
uint8_t *unicode_prop_Other_Math_table;
uint8_t *unicode_prop_Other_Alphabetic_table;
uint8_t *unicode_prop_Other_Lowercase_table;
uint8_t *unicode_prop_Other_Uppercase_table;
uint8_t *unicode_prop_Other_Grapheme_Extend_table;
uint8_t *unicode_prop_Other_Default_Ignorable_Code_Point_table;
uint8_t *unicode_prop_Other_ID_Start_table;
uint8_t *unicode_prop_Other_ID_Continue_table;
uint8_t *unicode_prop_Prepended_Concatenation_Mark_table;
uint8_t *unicode_prop_XID_Start1_table;
uint8_t *unicode_prop_XID_Continue1_table;
uint8_t *unicode_prop_Changes_When_Titlecased1_table;
uint8_t *unicode_prop_Changes_When_Casefolded1_table;
uint8_t *unicode_prop_Changes_When_NFKC_Casefolded1_table;
uint8_t *unicode_prop_Basic_Emoji1_table;
uint8_t *unicode_prop_Basic_Emoji2_table;
uint8_t *unicode_prop_RGI_Emoji_Modifier_Sequence_table;
uint8_t *unicode_prop_RGI_Emoji_Flag_Sequence_table;
uint8_t *unicode_prop_Emoji_Keycap_Sequence_table;
uint8_t *unicode_prop_ASCII_Hex_Digit_table;
uint8_t *unicode_prop_Bidi_Control_table;
uint8_t *unicode_prop_Dash_table;
uint8_t *unicode_prop_Deprecated_table;
uint8_t *unicode_prop_Diacritic_table;
uint8_t *unicode_prop_Extender_table;
uint8_t *unicode_prop_Hex_Digit_table;
uint8_t *unicode_prop_IDS_Unary_Operator_table;
uint8_t *unicode_prop_IDS_Binary_Operator_table;
uint8_t *unicode_prop_IDS_Trinary_Operator_table;
uint8_t *unicode_prop_Ideographic_table;
uint8_t *unicode_prop_Join_Control_table;
uint8_t *unicode_prop_Logical_Order_Exception_table;
uint8_t *unicode_prop_Modifier_Combining_Mark_table;
uint8_t *unicode_prop_Noncharacter_Code_Point_table;
uint8_t *unicode_prop_Pattern_Syntax_table;
uint8_t *unicode_prop_Pattern_White_Space_table;
uint8_t *unicode_prop_Quotation_Mark_table;
uint8_t *unicode_prop_Radical_table;
uint8_t *unicode_prop_Regional_Indicator_table;
uint8_t *unicode_prop_Sentence_Terminal_table;
uint8_t *unicode_prop_Soft_Dotted_table;
uint8_t *unicode_prop_Terminal_Punctuation_table;
uint8_t *unicode_prop_Unified_Ideograph_table;
uint8_t *unicode_prop_Variation_Selector_table;
uint8_t *unicode_prop_White_Space_table;
uint8_t *unicode_prop_Bidi_Mirrored_table;
uint8_t *unicode_prop_Emoji_table;
uint8_t *unicode_prop_Emoji_Component_table;
uint8_t *unicode_prop_Emoji_Modifier_table;
uint8_t *unicode_prop_Emoji_Modifier_Base_table;
uint8_t *unicode_prop_Emoji_Presentation_table;
uint8_t *unicode_prop_Extended_Pictographic_table;
uint8_t *unicode_prop_Default_Ignorable_Code_Point_table;
uint8_t **unicode_prop_table;
uint16_t *unicode_prop_len_table;
uint8_t *unicode_rgi_emoji_tag_sequence;
uint8_t *unicode_rgi_emoji_zwj_sequence;
char *unicode_script_name_table;
char *unicode_gc_name_table;
char *unicode_prop_name_table;
char *unicode_sequence_prop_name_table;


void fill_unicode_prop_table(const uint8_t **p) {
    p[0] = unicode_prop_Hyphen_table;
    p[1] = unicode_prop_Other_Math_table;
    p[2] = unicode_prop_Other_Alphabetic_table;
    p[3] = unicode_prop_Other_Lowercase_table;
    p[4] = unicode_prop_Other_Uppercase_table;
    p[5] = unicode_prop_Other_Grapheme_Extend_table;
    p[6] = unicode_prop_Other_Default_Ignorable_Code_Point_table;
    p[7] = unicode_prop_Other_ID_Start_table;
    p[8] = unicode_prop_Other_ID_Continue_table;
    p[9] = unicode_prop_Prepended_Concatenation_Mark_table;
    p[10] = unicode_prop_ID_Continue1_table;
    p[11] = unicode_prop_XID_Start1_table;
    p[12] = unicode_prop_XID_Continue1_table;
    p[13] = unicode_prop_Changes_When_Titlecased1_table;
    p[14] = unicode_prop_Changes_When_Casefolded1_table;
    p[15] = unicode_prop_Changes_When_NFKC_Casefolded1_table;
    p[16] = unicode_prop_Basic_Emoji1_table;
    p[17] = unicode_prop_Basic_Emoji2_table;
    p[18] = unicode_prop_RGI_Emoji_Modifier_Sequence_table;
    p[19] = unicode_prop_RGI_Emoji_Flag_Sequence_table;
    p[20] = unicode_prop_Emoji_Keycap_Sequence_table;
    p[21] = unicode_prop_ASCII_Hex_Digit_table;
    p[22] = unicode_prop_Bidi_Control_table;
    p[23] = unicode_prop_Dash_table;
    p[24] = unicode_prop_Deprecated_table;
    p[25] = unicode_prop_Diacritic_table;
    p[26] = unicode_prop_Extender_table;
    p[27] = unicode_prop_Hex_Digit_table;
    p[28] = unicode_prop_IDS_Unary_Operator_table;
    p[29] = unicode_prop_IDS_Binary_Operator_table;
    p[30] = unicode_prop_IDS_Trinary_Operator_table;
    p[31] = unicode_prop_Ideographic_table;
    p[32] = unicode_prop_Join_Control_table;
    p[33] = unicode_prop_Logical_Order_Exception_table;
    p[34] = unicode_prop_Modifier_Combining_Mark_table;
    p[35] = unicode_prop_Noncharacter_Code_Point_table;
    p[36] = unicode_prop_Pattern_Syntax_table;
    p[37] = unicode_prop_Pattern_White_Space_table;
    p[38] = unicode_prop_Quotation_Mark_table;
    p[39] = unicode_prop_Radical_table;
    p[40] = unicode_prop_Regional_Indicator_table;
    p[41] = unicode_prop_Sentence_Terminal_table;
    p[42] = unicode_prop_Soft_Dotted_table;
    p[43] = unicode_prop_Terminal_Punctuation_table;
    p[44] = unicode_prop_Unified_Ideograph_table;
    p[45] = unicode_prop_Variation_Selector_table;
    p[46] = unicode_prop_White_Space_table;
    p[47] = unicode_prop_Bidi_Mirrored_table;
    p[48] = unicode_prop_Emoji_table;
    p[49] = unicode_prop_Emoji_Component_table;
    p[50] = unicode_prop_Emoji_Modifier_table;
    p[51] = unicode_prop_Emoji_Modifier_Base_table;
    p[52] = unicode_prop_Emoji_Presentation_table;
    p[53] = unicode_prop_Extended_Pictographic_table;
    p[54] = unicode_prop_Default_Ignorable_Code_Point_table;
    p[55] = unicode_prop_ID_Start_table;
    p[56] = unicode_prop_Case_Ignorable_table;
}

#define countof(arr) ARRAY_LEN(arr)
void fill_unicode_prop_len_table(uint16_t *p) {
    p[0] = countof(unicode_prop_Hyphen_table);
    p[1] = countof(unicode_prop_Other_Math_table);
    p[2] = countof(unicode_prop_Other_Alphabetic_table);
    p[3] = countof(unicode_prop_Other_Lowercase_table);
    p[4] = countof(unicode_prop_Other_Uppercase_table);
    p[5] = countof(unicode_prop_Other_Grapheme_Extend_table);
    p[6] = countof(unicode_prop_Other_Default_Ignorable_Code_Point_table);
    p[7] = countof(unicode_prop_Other_ID_Start_table);
    p[8] = countof(unicode_prop_Other_ID_Continue_table);
    p[9] = countof(unicode_prop_Prepended_Concatenation_Mark_table);
    p[10] = countof(unicode_prop_ID_Continue1_table);
    p[11] = countof(unicode_prop_XID_Start1_table);
    p[12] = countof(unicode_prop_XID_Continue1_table);
    p[13] = countof(unicode_prop_Changes_When_Titlecased1_table);
    p[14] = countof(unicode_prop_Changes_When_Casefolded1_table);
    p[15] = countof(unicode_prop_Changes_When_NFKC_Casefolded1_table);
    p[16] = countof(unicode_prop_Basic_Emoji1_table);
    p[17] = countof(unicode_prop_Basic_Emoji2_table);
    p[18] = countof(unicode_prop_RGI_Emoji_Modifier_Sequence_table);
    p[19] = countof(unicode_prop_RGI_Emoji_Flag_Sequence_table);
    p[20] = countof(unicode_prop_Emoji_Keycap_Sequence_table);
    p[21] = countof(unicode_prop_ASCII_Hex_Digit_table);
    p[22] = countof(unicode_prop_Bidi_Control_table);
    p[23] = countof(unicode_prop_Dash_table);
    p[24] = countof(unicode_prop_Deprecated_table);
    p[25] = countof(unicode_prop_Diacritic_table);
    p[26] = countof(unicode_prop_Extender_table);
    p[27] = countof(unicode_prop_Hex_Digit_table);
    p[28] = countof(unicode_prop_IDS_Unary_Operator_table);
    p[29] = countof(unicode_prop_IDS_Binary_Operator_table);
    p[30] = countof(unicode_prop_IDS_Trinary_Operator_table);
    p[31] = countof(unicode_prop_Ideographic_table);
    p[32] = countof(unicode_prop_Join_Control_table);
    p[33] = countof(unicode_prop_Logical_Order_Exception_table);
    p[34] = countof(unicode_prop_Modifier_Combining_Mark_table);
    p[35] = countof(unicode_prop_Noncharacter_Code_Point_table);
    p[36] = countof(unicode_prop_Pattern_Syntax_table);
    p[37] = countof(unicode_prop_Pattern_White_Space_table);
    p[38] = countof(unicode_prop_Quotation_Mark_table);
    p[39] = countof(unicode_prop_Radical_table);
    p[40] = countof(unicode_prop_Regional_Indicator_table);
    p[41] = countof(unicode_prop_Sentence_Terminal_table);
    p[42] = countof(unicode_prop_Soft_Dotted_table);
    p[43] = countof(unicode_prop_Terminal_Punctuation_table);
    p[44] = countof(unicode_prop_Unified_Ideograph_table);
    p[45] = countof(unicode_prop_Variation_Selector_table);
    p[46] = countof(unicode_prop_White_Space_table);
    p[47] = countof(unicode_prop_Bidi_Mirrored_table);
    p[48] = countof(unicode_prop_Emoji_table);
    p[49] = countof(unicode_prop_Emoji_Component_table);
    p[50] = countof(unicode_prop_Emoji_Modifier_table);
    p[51] = countof(unicode_prop_Emoji_Modifier_Base_table);
    p[52] = countof(unicode_prop_Emoji_Presentation_table);
    p[53] = countof(unicode_prop_Extended_Pictographic_table);
    p[54] = countof(unicode_prop_Default_Ignorable_Code_Point_table);
    p[55] = countof(unicode_prop_ID_Start_table);
    p[56] = countof(unicode_prop_Case_Ignorable_table);
}

int unicode_alloc_tables(void) {
    int ok = 1;
    case_conv_table1 = (uint32_t*)ft_malloc(378 * sizeof(uint32_t));
    if (!case_conv_table1) { ok = 0; goto cleanup; }
    fill_case_conv_table1(case_conv_table1);
    case_conv_table2 = (uint8_t*)ft_malloc(378 * sizeof(uint8_t));
    if (!case_conv_table2) { ok = 0; goto cleanup; }
    fill_case_conv_table2(case_conv_table2);
    case_conv_ext = (uint16_t*)ft_malloc(58 * sizeof(uint16_t));
    if (!case_conv_ext) { ok = 0; goto cleanup; }
    fill_case_conv_ext(case_conv_ext);
    unicode_prop_Cased1_table = (uint8_t*)ft_malloc(190 * sizeof(uint8_t));
    if (!unicode_prop_Cased1_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Cased1_table(unicode_prop_Cased1_table);
    unicode_prop_Cased1_index = (uint8_t*)ft_malloc(18 * sizeof(uint8_t));
    if (!unicode_prop_Cased1_index) { ok = 0; goto cleanup; }
    fill_unicode_prop_Cased1_index(unicode_prop_Cased1_index);
    unicode_prop_Case_Ignorable_table = (uint8_t*)ft_malloc(785 * sizeof(uint8_t));
    if (!unicode_prop_Case_Ignorable_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Case_Ignorable_table(unicode_prop_Case_Ignorable_table);
    unicode_prop_Case_Ignorable_index = (uint8_t*)ft_malloc(75 * sizeof(uint8_t));
    if (!unicode_prop_Case_Ignorable_index) { ok = 0; goto cleanup; }
    fill_unicode_prop_Case_Ignorable_index(unicode_prop_Case_Ignorable_index);
    unicode_prop_ID_Start_table = (uint8_t*)ft_malloc(1146 * sizeof(uint8_t));
    if (!unicode_prop_ID_Start_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_ID_Start_table(unicode_prop_ID_Start_table);
    unicode_prop_ID_Start_index = (uint8_t*)ft_malloc(108 * sizeof(uint8_t));
    if (!unicode_prop_ID_Start_index) { ok = 0; goto cleanup; }
    fill_unicode_prop_ID_Start_index(unicode_prop_ID_Start_index);
    unicode_prop_ID_Continue1_table = (uint8_t*)ft_malloc(708 * sizeof(uint8_t));
    if (!unicode_prop_ID_Continue1_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_ID_Continue1_table(unicode_prop_ID_Continue1_table);
    unicode_prop_ID_Continue1_index = (uint8_t*)ft_malloc(66 * sizeof(uint8_t));
    if (!unicode_prop_ID_Continue1_index) { ok = 0; goto cleanup; }
    fill_unicode_prop_ID_Continue1_index(unicode_prop_ID_Continue1_index);
    unicode_cc_table = (uint8_t*)ft_malloc(937 * sizeof(uint8_t));
    if (!unicode_cc_table) { ok = 0; goto cleanup; }
    fill_unicode_cc_table(unicode_cc_table);
    unicode_cc_index = (uint8_t*)ft_malloc(90 * sizeof(uint8_t));
    if (!unicode_cc_index) { ok = 0; goto cleanup; }
    fill_unicode_cc_index(unicode_cc_index);
    unicode_decomp_table1 = (uint32_t*)ft_malloc(709 * sizeof(uint32_t));
    if (!unicode_decomp_table1) { ok = 0; goto cleanup; }
    fill_unicode_decomp_table1(unicode_decomp_table1);
    unicode_decomp_table2 = (uint16_t*)ft_malloc(709 * sizeof(uint16_t));
    if (!unicode_decomp_table2) { ok = 0; goto cleanup; }
    fill_unicode_decomp_table2(unicode_decomp_table2);
    unicode_decomp_data = (uint8_t*)ft_malloc(9452 * sizeof(uint8_t));
    if (!unicode_decomp_data) { ok = 0; goto cleanup; }
    fill_unicode_decomp_data(unicode_decomp_data);
    unicode_comp_table = (uint16_t*)ft_malloc(965 * sizeof(uint16_t));
    if (!unicode_comp_table) { ok = 0; goto cleanup; }
    fill_unicode_comp_table(unicode_comp_table);
    unicode_gc_table = (uint8_t*)ft_malloc(4122 * sizeof(uint8_t));
    if (!unicode_gc_table) { ok = 0; goto cleanup; }
    fill_unicode_gc_table(unicode_gc_table);
    unicode_script_table = (uint8_t*)ft_malloc(2818 * sizeof(uint8_t));
    if (!unicode_script_table) { ok = 0; goto cleanup; }
    fill_unicode_script_table(unicode_script_table);
    unicode_script_ext_table = (uint8_t*)ft_malloc(1278 * sizeof(uint8_t));
    if (!unicode_script_ext_table) { ok = 0; goto cleanup; }
    fill_unicode_script_ext_table(unicode_script_ext_table);
    unicode_prop_Hyphen_table = (uint8_t*)ft_malloc(28 * sizeof(uint8_t));
    if (!unicode_prop_Hyphen_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Hyphen_table(unicode_prop_Hyphen_table);
    unicode_prop_Other_Math_table = (uint8_t*)ft_malloc(200 * sizeof(uint8_t));
    if (!unicode_prop_Other_Math_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Other_Math_table(unicode_prop_Other_Math_table);
    unicode_prop_Other_Alphabetic_table = (uint8_t*)ft_malloc(452 * sizeof(uint8_t));
    if (!unicode_prop_Other_Alphabetic_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Other_Alphabetic_table(unicode_prop_Other_Alphabetic_table);
    unicode_prop_Other_Lowercase_table = (uint8_t*)ft_malloc(68 * sizeof(uint8_t));
    if (!unicode_prop_Other_Lowercase_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Other_Lowercase_table(unicode_prop_Other_Lowercase_table);
    unicode_prop_Other_Uppercase_table = (uint8_t*)ft_malloc(15 * sizeof(uint8_t));
    if (!unicode_prop_Other_Uppercase_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Other_Uppercase_table(unicode_prop_Other_Uppercase_table);
    unicode_prop_Other_Grapheme_Extend_table = (uint8_t*)ft_malloc(112 * sizeof(uint8_t));
    if (!unicode_prop_Other_Grapheme_Extend_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Other_Grapheme_Extend_table(unicode_prop_Other_Grapheme_Extend_table);
    unicode_prop_Other_Default_Ignorable_Code_Point_table = (uint8_t*)ft_malloc(32 * sizeof(uint8_t));
    if (!unicode_prop_Other_Default_Ignorable_Code_Point_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Other_Default_Ignorable_Code_Point_table(unicode_prop_Other_Default_Ignorable_Code_Point_table);
    unicode_prop_Other_ID_Start_table = (uint8_t*)ft_malloc(11 * sizeof(uint8_t));
    if (!unicode_prop_Other_ID_Start_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Other_ID_Start_table(unicode_prop_Other_ID_Start_table);
    unicode_prop_Other_ID_Continue_table = (uint8_t*)ft_malloc(22 * sizeof(uint8_t));
    if (!unicode_prop_Other_ID_Continue_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Other_ID_Continue_table(unicode_prop_Other_ID_Continue_table);
    unicode_prop_Prepended_Concatenation_Mark_table = (uint8_t*)ft_malloc(19 * sizeof(uint8_t));
    if (!unicode_prop_Prepended_Concatenation_Mark_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Prepended_Concatenation_Mark_table(unicode_prop_Prepended_Concatenation_Mark_table);
    unicode_prop_XID_Start1_table = (uint8_t*)ft_malloc(31 * sizeof(uint8_t));
    if (!unicode_prop_XID_Start1_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_XID_Start1_table(unicode_prop_XID_Start1_table);
    unicode_prop_XID_Continue1_table = (uint8_t*)ft_malloc(23 * sizeof(uint8_t));
    if (!unicode_prop_XID_Continue1_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_XID_Continue1_table(unicode_prop_XID_Continue1_table);
    unicode_prop_Changes_When_Titlecased1_table = (uint8_t*)ft_malloc(22 * sizeof(uint8_t));
    if (!unicode_prop_Changes_When_Titlecased1_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Changes_When_Titlecased1_table(unicode_prop_Changes_When_Titlecased1_table);
    unicode_prop_Changes_When_Casefolded1_table = (uint8_t*)ft_malloc(29 * sizeof(uint8_t));
    if (!unicode_prop_Changes_When_Casefolded1_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Changes_When_Casefolded1_table(unicode_prop_Changes_When_Casefolded1_table);
    unicode_prop_Changes_When_NFKC_Casefolded1_table = (uint8_t*)ft_malloc(449 * sizeof(uint8_t));
    if (!unicode_prop_Changes_When_NFKC_Casefolded1_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Changes_When_NFKC_Casefolded1_table(unicode_prop_Changes_When_NFKC_Casefolded1_table);
    unicode_prop_Basic_Emoji1_table = (uint8_t*)ft_malloc(144 * sizeof(uint8_t));
    if (!unicode_prop_Basic_Emoji1_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Basic_Emoji1_table(unicode_prop_Basic_Emoji1_table);
    unicode_prop_Basic_Emoji2_table = (uint8_t*)ft_malloc(183 * sizeof(uint8_t));
    if (!unicode_prop_Basic_Emoji2_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Basic_Emoji2_table(unicode_prop_Basic_Emoji2_table);
    unicode_prop_RGI_Emoji_Modifier_Sequence_table = (uint8_t*)ft_malloc(72 * sizeof(uint8_t));
    if (!unicode_prop_RGI_Emoji_Modifier_Sequence_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_RGI_Emoji_Modifier_Sequence_table(unicode_prop_RGI_Emoji_Modifier_Sequence_table);
    unicode_prop_RGI_Emoji_Flag_Sequence_table = (uint8_t*)ft_malloc(128 * sizeof(uint8_t));
    if (!unicode_prop_RGI_Emoji_Flag_Sequence_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_RGI_Emoji_Flag_Sequence_table(unicode_prop_RGI_Emoji_Flag_Sequence_table);
    unicode_prop_Emoji_Keycap_Sequence_table = (uint8_t*)ft_malloc(4 * sizeof(uint8_t));
    if (!unicode_prop_Emoji_Keycap_Sequence_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Emoji_Keycap_Sequence_table(unicode_prop_Emoji_Keycap_Sequence_table);
    unicode_prop_ASCII_Hex_Digit_table = (uint8_t*)ft_malloc(5 * sizeof(uint8_t));
    if (!unicode_prop_ASCII_Hex_Digit_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_ASCII_Hex_Digit_table(unicode_prop_ASCII_Hex_Digit_table);
    unicode_prop_Bidi_Control_table = (uint8_t*)ft_malloc(10 * sizeof(uint8_t));
    if (!unicode_prop_Bidi_Control_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Bidi_Control_table(unicode_prop_Bidi_Control_table);
    unicode_prop_Dash_table = (uint8_t*)ft_malloc(58 * sizeof(uint8_t));
    if (!unicode_prop_Dash_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Dash_table(unicode_prop_Dash_table);
    unicode_prop_Deprecated_table = (uint8_t*)ft_malloc(23 * sizeof(uint8_t));
    if (!unicode_prop_Deprecated_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Deprecated_table(unicode_prop_Deprecated_table);
    unicode_prop_Diacritic_table = (uint8_t*)ft_malloc(447 * sizeof(uint8_t));
    if (!unicode_prop_Diacritic_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Diacritic_table(unicode_prop_Diacritic_table);
    unicode_prop_Extender_table = (uint8_t*)ft_malloc(116 * sizeof(uint8_t));
    if (!unicode_prop_Extender_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Extender_table(unicode_prop_Extender_table);
    unicode_prop_Hex_Digit_table = (uint8_t*)ft_malloc(12 * sizeof(uint8_t));
    if (!unicode_prop_Hex_Digit_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Hex_Digit_table(unicode_prop_Hex_Digit_table);
    unicode_prop_IDS_Unary_Operator_table = (uint8_t*)ft_malloc(4 * sizeof(uint8_t));
    if (!unicode_prop_IDS_Unary_Operator_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_IDS_Unary_Operator_table(unicode_prop_IDS_Unary_Operator_table);
    unicode_prop_IDS_Binary_Operator_table = (uint8_t*)ft_malloc(8 * sizeof(uint8_t));
    if (!unicode_prop_IDS_Binary_Operator_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_IDS_Binary_Operator_table(unicode_prop_IDS_Binary_Operator_table);
    unicode_prop_IDS_Trinary_Operator_table = (uint8_t*)ft_malloc(4 * sizeof(uint8_t));
    if (!unicode_prop_IDS_Trinary_Operator_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_IDS_Trinary_Operator_table(unicode_prop_IDS_Trinary_Operator_table);
    unicode_prop_Ideographic_table = (uint8_t*)ft_malloc(71 * sizeof(uint8_t));
    if (!unicode_prop_Ideographic_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Ideographic_table(unicode_prop_Ideographic_table);
    unicode_prop_Join_Control_table = (uint8_t*)ft_malloc(4 * sizeof(uint8_t));
    if (!unicode_prop_Join_Control_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Join_Control_table(unicode_prop_Join_Control_table);
    unicode_prop_Logical_Order_Exception_table = (uint8_t*)ft_malloc(15 * sizeof(uint8_t));
    if (!unicode_prop_Logical_Order_Exception_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Logical_Order_Exception_table(unicode_prop_Logical_Order_Exception_table);
    unicode_prop_Modifier_Combining_Mark_table = (uint8_t*)ft_malloc(16 * sizeof(uint8_t));
    if (!unicode_prop_Modifier_Combining_Mark_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Modifier_Combining_Mark_table(unicode_prop_Modifier_Combining_Mark_table);
    unicode_prop_Noncharacter_Code_Point_table = (uint8_t*)ft_malloc(71 * sizeof(uint8_t));
    if (!unicode_prop_Noncharacter_Code_Point_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Noncharacter_Code_Point_table(unicode_prop_Noncharacter_Code_Point_table);
    unicode_prop_Pattern_Syntax_table = (uint8_t*)ft_malloc(58 * sizeof(uint8_t));
    if (!unicode_prop_Pattern_Syntax_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Pattern_Syntax_table(unicode_prop_Pattern_Syntax_table);
    unicode_prop_Pattern_White_Space_table = (uint8_t*)ft_malloc(11 * sizeof(uint8_t));
    if (!unicode_prop_Pattern_White_Space_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Pattern_White_Space_table(unicode_prop_Pattern_White_Space_table);
    unicode_prop_Quotation_Mark_table = (uint8_t*)ft_malloc(31 * sizeof(uint8_t));
    if (!unicode_prop_Quotation_Mark_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Quotation_Mark_table(unicode_prop_Quotation_Mark_table);
    unicode_prop_Radical_table = (uint8_t*)ft_malloc(9 * sizeof(uint8_t));
    if (!unicode_prop_Radical_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Radical_table(unicode_prop_Radical_table);
    unicode_prop_Regional_Indicator_table = (uint8_t*)ft_malloc(4 * sizeof(uint8_t));
    if (!unicode_prop_Regional_Indicator_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Regional_Indicator_table(unicode_prop_Regional_Indicator_table);
    unicode_prop_Sentence_Terminal_table = (uint8_t*)ft_malloc(213 * sizeof(uint8_t));
    if (!unicode_prop_Sentence_Terminal_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Sentence_Terminal_table(unicode_prop_Sentence_Terminal_table);
    unicode_prop_Soft_Dotted_table = (uint8_t*)ft_malloc(79 * sizeof(uint8_t));
    if (!unicode_prop_Soft_Dotted_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Soft_Dotted_table(unicode_prop_Soft_Dotted_table);
    unicode_prop_Terminal_Punctuation_table = (uint8_t*)ft_malloc(264 * sizeof(uint8_t));
    if (!unicode_prop_Terminal_Punctuation_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Terminal_Punctuation_table(unicode_prop_Terminal_Punctuation_table);
    unicode_prop_Unified_Ideograph_table = (uint8_t*)ft_malloc(46 * sizeof(uint8_t));
    if (!unicode_prop_Unified_Ideograph_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Unified_Ideograph_table(unicode_prop_Unified_Ideograph_table);
    unicode_prop_Variation_Selector_table = (uint8_t*)ft_malloc(13 * sizeof(uint8_t));
    if (!unicode_prop_Variation_Selector_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Variation_Selector_table(unicode_prop_Variation_Selector_table);
    unicode_prop_White_Space_table = (uint8_t*)ft_malloc(22 * sizeof(uint8_t));
    if (!unicode_prop_White_Space_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_White_Space_table(unicode_prop_White_Space_table);
    unicode_prop_Bidi_Mirrored_table = (uint8_t*)ft_malloc(173 * sizeof(uint8_t));
    if (!unicode_prop_Bidi_Mirrored_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Bidi_Mirrored_table(unicode_prop_Bidi_Mirrored_table);
    unicode_prop_Emoji_table = (uint8_t*)ft_malloc(239 * sizeof(uint8_t));
    if (!unicode_prop_Emoji_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Emoji_table(unicode_prop_Emoji_table);
    unicode_prop_Emoji_Component_table = (uint8_t*)ft_malloc(28 * sizeof(uint8_t));
    if (!unicode_prop_Emoji_Component_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Emoji_Component_table(unicode_prop_Emoji_Component_table);
    unicode_prop_Emoji_Modifier_table = (uint8_t*)ft_malloc(4 * sizeof(uint8_t));
    if (!unicode_prop_Emoji_Modifier_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Emoji_Modifier_table(unicode_prop_Emoji_Modifier_table);
    unicode_prop_Emoji_Modifier_Base_table = (uint8_t*)ft_malloc(71 * sizeof(uint8_t));
    if (!unicode_prop_Emoji_Modifier_Base_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Emoji_Modifier_Base_table(unicode_prop_Emoji_Modifier_Base_table);
    unicode_prop_Emoji_Presentation_table = (uint8_t*)ft_malloc(145 * sizeof(uint8_t));
    if (!unicode_prop_Emoji_Presentation_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Emoji_Presentation_table(unicode_prop_Emoji_Presentation_table);
    unicode_prop_Extended_Pictographic_table = (uint8_t*)ft_malloc(254 * sizeof(uint8_t));
    if (!unicode_prop_Extended_Pictographic_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Extended_Pictographic_table(unicode_prop_Extended_Pictographic_table);
    unicode_prop_Default_Ignorable_Code_Point_table = (uint8_t*)ft_malloc(51 * sizeof(uint8_t));
    if (!unicode_prop_Default_Ignorable_Code_Point_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_Default_Ignorable_Code_Point_table(unicode_prop_Default_Ignorable_Code_Point_table);
    unicode_prop_table = (const uint8_t**)ft_malloc(57 * sizeof(const uint8_t*));
    if (!unicode_prop_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_table(unicode_prop_table);
    unicode_prop_len_table = (uint16_t*)ft_malloc(0 * sizeof(uint16_t));
    if (!unicode_prop_len_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_len_table(unicode_prop_len_table);
    unicode_rgi_emoji_tag_sequence = (uint8_t*)ft_malloc(18 * sizeof(uint8_t));
    if (!unicode_rgi_emoji_tag_sequence) { ok = 0; goto cleanup; }
    fill_unicode_rgi_emoji_tag_sequence(unicode_rgi_emoji_tag_sequence);
    unicode_rgi_emoji_zwj_sequence = (uint8_t*)ft_malloc(2392 * sizeof(uint8_t));
    if (!unicode_rgi_emoji_zwj_sequence) { ok = 0; goto cleanup; }
    fill_unicode_rgi_emoji_zwj_sequence(unicode_rgi_emoji_zwj_sequence);
	unicode_gc_name_table = (char*)ft_malloc(ARRAY_LEN_unicode_gc_name_table * sizeof(char));
    if (!unicode_gc_name_table) { ok = 0; goto cleanup; }
    fill_unicode_gc_name_table(unicode_gc_name_table);
    unicode_script_name_table = (char*)ft_malloc(ARRAY_LEN_unicode_script_name_table * sizeof(char));
    if (!unicode_script_name_table) { ok = 0; goto cleanup; }
    fill_unicode_script_name_table(unicode_script_name_table);
    unicode_prop_name_table = (char*)ft_malloc(ARRAY_LEN_unicode_prop_name_table * sizeof(char));
    if (!unicode_prop_name_table) { ok = 0; goto cleanup; }
    fill_unicode_prop_name_table(unicode_prop_name_table);
    unicode_sequence_prop_name_table = (char*)ft_malloc(ARRAY_LEN_unicode_sequence_prop_name_table * sizeof(char));
    if (!unicode_sequence_prop_name_table) { ok = 0; goto cleanup; }
    fill_unicode_sequence_prop_name_table(unicode_sequence_prop_name_table);
    return 1;
cleanup:
    unicode_free_tables();
    return 0;
}

void unicode_free_tables(void) {
    if (case_conv_table1) { ft_free(case_conv_table1); case_conv_table1 = NULL; }
    if (case_conv_table2) { ft_free(case_conv_table2); case_conv_table2 = NULL; }
    if (case_conv_ext) { ft_free(case_conv_ext); case_conv_ext = NULL; }
    if (unicode_prop_Cased1_table) { ft_free(unicode_prop_Cased1_table); unicode_prop_Cased1_table = NULL; }
    if (unicode_prop_Cased1_index) { ft_free(unicode_prop_Cased1_index); unicode_prop_Cased1_index = NULL; }
    if (unicode_prop_Case_Ignorable_table) { ft_free(unicode_prop_Case_Ignorable_table); unicode_prop_Case_Ignorable_table = NULL; }
    if (unicode_prop_Case_Ignorable_index) { ft_free(unicode_prop_Case_Ignorable_index); unicode_prop_Case_Ignorable_index = NULL; }
    if (unicode_prop_ID_Start_table) { ft_free(unicode_prop_ID_Start_table); unicode_prop_ID_Start_table = NULL; }
    if (unicode_prop_ID_Start_index) { ft_free(unicode_prop_ID_Start_index); unicode_prop_ID_Start_index = NULL; }
    if (unicode_prop_ID_Continue1_table) { ft_free(unicode_prop_ID_Continue1_table); unicode_prop_ID_Continue1_table = NULL; }
    if (unicode_prop_ID_Continue1_index) { ft_free(unicode_prop_ID_Continue1_index); unicode_prop_ID_Continue1_index = NULL; }
    if (unicode_cc_table) { ft_free(unicode_cc_table); unicode_cc_table = NULL; }
    if (unicode_cc_index) { ft_free(unicode_cc_index); unicode_cc_index = NULL; }
    if (unicode_decomp_table1) { ft_free(unicode_decomp_table1); unicode_decomp_table1 = NULL; }
    if (unicode_decomp_table2) { ft_free(unicode_decomp_table2); unicode_decomp_table2 = NULL; }
    if (unicode_decomp_data) { ft_free(unicode_decomp_data); unicode_decomp_data = NULL; }
    if (unicode_comp_table) { ft_free(unicode_comp_table); unicode_comp_table = NULL; }
    if (unicode_gc_table) { ft_free(unicode_gc_table); unicode_gc_table = NULL; }
    if (unicode_script_table) { ft_free(unicode_script_table); unicode_script_table = NULL; }
    if (unicode_script_ext_table) { ft_free(unicode_script_ext_table); unicode_script_ext_table = NULL; }
    if (unicode_prop_Hyphen_table) { ft_free(unicode_prop_Hyphen_table); unicode_prop_Hyphen_table = NULL; }
    if (unicode_prop_Other_Math_table) { ft_free(unicode_prop_Other_Math_table); unicode_prop_Other_Math_table = NULL; }
    if (unicode_prop_Other_Alphabetic_table) { ft_free(unicode_prop_Other_Alphabetic_table); unicode_prop_Other_Alphabetic_table = NULL; }
    if (unicode_prop_Other_Lowercase_table) { ft_free(unicode_prop_Other_Lowercase_table); unicode_prop_Other_Lowercase_table = NULL; }
    if (unicode_prop_Other_Uppercase_table) { ft_free(unicode_prop_Other_Uppercase_table); unicode_prop_Other_Uppercase_table = NULL; }
    if (unicode_prop_Other_Grapheme_Extend_table) { ft_free(unicode_prop_Other_Grapheme_Extend_table); unicode_prop_Other_Grapheme_Extend_table = NULL; }
    if (unicode_prop_Other_Default_Ignorable_Code_Point_table) { ft_free(unicode_prop_Other_Default_Ignorable_Code_Point_table); unicode_prop_Other_Default_Ignorable_Code_Point_table = NULL; }
    if (unicode_prop_Other_ID_Start_table) { ft_free(unicode_prop_Other_ID_Start_table); unicode_prop_Other_ID_Start_table = NULL; }
    if (unicode_prop_Other_ID_Continue_table) { ft_free(unicode_prop_Other_ID_Continue_table); unicode_prop_Other_ID_Continue_table = NULL; }
    if (unicode_prop_Prepended_Concatenation_Mark_table) { ft_free(unicode_prop_Prepended_Concatenation_Mark_table); unicode_prop_Prepended_Concatenation_Mark_table = NULL; }
    if (unicode_prop_XID_Start1_table) { ft_free(unicode_prop_XID_Start1_table); unicode_prop_XID_Start1_table = NULL; }
    if (unicode_prop_XID_Continue1_table) { ft_free(unicode_prop_XID_Continue1_table); unicode_prop_XID_Continue1_table = NULL; }
    if (unicode_prop_Changes_When_Titlecased1_table) { ft_free(unicode_prop_Changes_When_Titlecased1_table); unicode_prop_Changes_When_Titlecased1_table = NULL; }
    if (unicode_prop_Changes_When_Casefolded1_table) { ft_free(unicode_prop_Changes_When_Casefolded1_table); unicode_prop_Changes_When_Casefolded1_table = NULL; }
    if (unicode_prop_Changes_When_NFKC_Casefolded1_table) { ft_free(unicode_prop_Changes_When_NFKC_Casefolded1_table); unicode_prop_Changes_When_NFKC_Casefolded1_table = NULL; }
    if (unicode_prop_Basic_Emoji1_table) { ft_free(unicode_prop_Basic_Emoji1_table); unicode_prop_Basic_Emoji1_table = NULL; }
    if (unicode_prop_Basic_Emoji2_table) { ft_free(unicode_prop_Basic_Emoji2_table); unicode_prop_Basic_Emoji2_table = NULL; }
    if (unicode_prop_RGI_Emoji_Modifier_Sequence_table) { ft_free(unicode_prop_RGI_Emoji_Modifier_Sequence_table); unicode_prop_RGI_Emoji_Modifier_Sequence_table = NULL; }
    if (unicode_prop_RGI_Emoji_Flag_Sequence_table) { ft_free(unicode_prop_RGI_Emoji_Flag_Sequence_table); unicode_prop_RGI_Emoji_Flag_Sequence_table = NULL; }
    if (unicode_prop_Emoji_Keycap_Sequence_table) { ft_free(unicode_prop_Emoji_Keycap_Sequence_table); unicode_prop_Emoji_Keycap_Sequence_table = NULL; }
    if (unicode_prop_ASCII_Hex_Digit_table) { ft_free(unicode_prop_ASCII_Hex_Digit_table); unicode_prop_ASCII_Hex_Digit_table = NULL; }
    if (unicode_prop_Bidi_Control_table) { ft_free(unicode_prop_Bidi_Control_table); unicode_prop_Bidi_Control_table = NULL; }
    if (unicode_prop_Dash_table) { ft_free(unicode_prop_Dash_table); unicode_prop_Dash_table = NULL; }
    if (unicode_prop_Deprecated_table) { ft_free(unicode_prop_Deprecated_table); unicode_prop_Deprecated_table = NULL; }
    if (unicode_prop_Diacritic_table) { ft_free(unicode_prop_Diacritic_table); unicode_prop_Diacritic_table = NULL; }
    if (unicode_prop_Extender_table) { ft_free(unicode_prop_Extender_table); unicode_prop_Extender_table = NULL; }
    if (unicode_prop_Hex_Digit_table) { ft_free(unicode_prop_Hex_Digit_table); unicode_prop_Hex_Digit_table = NULL; }
    if (unicode_prop_IDS_Unary_Operator_table) { ft_free(unicode_prop_IDS_Unary_Operator_table); unicode_prop_IDS_Unary_Operator_table = NULL; }
    if (unicode_prop_IDS_Binary_Operator_table) { ft_free(unicode_prop_IDS_Binary_Operator_table); unicode_prop_IDS_Binary_Operator_table = NULL; }
    if (unicode_prop_IDS_Trinary_Operator_table) { ft_free(unicode_prop_IDS_Trinary_Operator_table); unicode_prop_IDS_Trinary_Operator_table = NULL; }
    if (unicode_prop_Ideographic_table) { ft_free(unicode_prop_Ideographic_table); unicode_prop_Ideographic_table = NULL; }
    if (unicode_prop_Join_Control_table) { ft_free(unicode_prop_Join_Control_table); unicode_prop_Join_Control_table = NULL; }
    if (unicode_prop_Logical_Order_Exception_table) { ft_free(unicode_prop_Logical_Order_Exception_table); unicode_prop_Logical_Order_Exception_table = NULL; }
    if (unicode_prop_Modifier_Combining_Mark_table) { ft_free(unicode_prop_Modifier_Combining_Mark_table); unicode_prop_Modifier_Combining_Mark_table = NULL; }
    if (unicode_prop_Noncharacter_Code_Point_table) { ft_free(unicode_prop_Noncharacter_Code_Point_table); unicode_prop_Noncharacter_Code_Point_table = NULL; }
    if (unicode_prop_Pattern_Syntax_table) { ft_free(unicode_prop_Pattern_Syntax_table); unicode_prop_Pattern_Syntax_table = NULL; }
    if (unicode_prop_Pattern_White_Space_table) { ft_free(unicode_prop_Pattern_White_Space_table); unicode_prop_Pattern_White_Space_table = NULL; }
    if (unicode_prop_Quotation_Mark_table) { ft_free(unicode_prop_Quotation_Mark_table); unicode_prop_Quotation_Mark_table = NULL; }
    if (unicode_prop_Radical_table) { ft_free(unicode_prop_Radical_table); unicode_prop_Radical_table = NULL; }
    if (unicode_prop_Regional_Indicator_table) { ft_free(unicode_prop_Regional_Indicator_table); unicode_prop_Regional_Indicator_table = NULL; }
    if (unicode_prop_Sentence_Terminal_table) { ft_free(unicode_prop_Sentence_Terminal_table); unicode_prop_Sentence_Terminal_table = NULL; }
    if (unicode_prop_Soft_Dotted_table) { ft_free(unicode_prop_Soft_Dotted_table); unicode_prop_Soft_Dotted_table = NULL; }
    if (unicode_prop_Terminal_Punctuation_table) { ft_free(unicode_prop_Terminal_Punctuation_table); unicode_prop_Terminal_Punctuation_table = NULL; }
    if (unicode_prop_Unified_Ideograph_table) { ft_free(unicode_prop_Unified_Ideograph_table); unicode_prop_Unified_Ideograph_table = NULL; }
    if (unicode_prop_Variation_Selector_table) { ft_free(unicode_prop_Variation_Selector_table); unicode_prop_Variation_Selector_table = NULL; }
    if (unicode_prop_White_Space_table) { ft_free(unicode_prop_White_Space_table); unicode_prop_White_Space_table = NULL; }
    if (unicode_prop_Bidi_Mirrored_table) { ft_free(unicode_prop_Bidi_Mirrored_table); unicode_prop_Bidi_Mirrored_table = NULL; }
    if (unicode_prop_Emoji_table) { ft_free(unicode_prop_Emoji_table); unicode_prop_Emoji_table = NULL; }
    if (unicode_prop_Emoji_Component_table) { ft_free(unicode_prop_Emoji_Component_table); unicode_prop_Emoji_Component_table = NULL; }
    if (unicode_prop_Emoji_Modifier_table) { ft_free(unicode_prop_Emoji_Modifier_table); unicode_prop_Emoji_Modifier_table = NULL; }
    if (unicode_prop_Emoji_Modifier_Base_table) { ft_free(unicode_prop_Emoji_Modifier_Base_table); unicode_prop_Emoji_Modifier_Base_table = NULL; }
    if (unicode_prop_Emoji_Presentation_table) { ft_free(unicode_prop_Emoji_Presentation_table); unicode_prop_Emoji_Presentation_table = NULL; }
    if (unicode_prop_Extended_Pictographic_table) { ft_free(unicode_prop_Extended_Pictographic_table); unicode_prop_Extended_Pictographic_table = NULL; }
    if (unicode_prop_Default_Ignorable_Code_Point_table) { ft_free(unicode_prop_Default_Ignorable_Code_Point_table); unicode_prop_Default_Ignorable_Code_Point_table = NULL; }
    if (unicode_prop_table) { ft_free(unicode_prop_table); unicode_prop_table = NULL; }
    if (unicode_prop_len_table) { ft_free(unicode_prop_len_table); unicode_prop_len_table = NULL; }
    if (unicode_rgi_emoji_tag_sequence) { ft_free(unicode_rgi_emoji_tag_sequence); unicode_rgi_emoji_tag_sequence = NULL; }
    if (unicode_rgi_emoji_zwj_sequence) { ft_free(unicode_rgi_emoji_zwj_sequence); unicode_rgi_emoji_zwj_sequence = NULL; }
	if (unicode_gc_name_table) { ft_free(unicode_gc_name_table); unicode_gc_name_table = NULL; }
    if (unicode_script_name_table) { ft_free(unicode_script_name_table); unicode_script_name_table = NULL; }
    if (unicode_prop_name_table) { ft_free(unicode_prop_name_table); unicode_prop_name_table = NULL; }
    if (unicode_sequence_prop_name_table) { ft_free(unicode_sequence_prop_name_table); unicode_sequence_prop_name_table = NULL; }
}
