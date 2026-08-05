/* Compressed unicode tables */
/* Automatically generated file - do not edit */

#ifndef LIBUNICODE_TABLE_H
#define LIBUNICODE_TABLE_H

#include <stdint.h>

/* 分配和释放所有动态表的函数 */
int unicode_alloc_tables(void);
void unicode_free_tables(void);

/* ========================================================================= */
/* 以下数组原本为静态常量，现改为 extern 指针（可写），以便堆分配            */
/* 每个数组都配有长度宏，供 countof 或 ARRAY_LEN 使用                       */
/* ========================================================================= */

/* --- 第一部分：无条件编译的数组（始终存在） --- */

extern uint32_t *case_conv_table1;
#define ARRAY_LEN_case_conv_table1 378

extern uint8_t *case_conv_table2;
#define ARRAY_LEN_case_conv_table2 378

extern uint16_t *case_conv_ext;
#define ARRAY_LEN_case_conv_ext 58

extern uint8_t *unicode_prop_Cased1_table;
#define ARRAY_LEN_unicode_prop_Cased1_table 190

extern uint8_t *unicode_prop_Cased1_index;
#define ARRAY_LEN_unicode_prop_Cased1_index 18

extern uint8_t *unicode_prop_Case_Ignorable_table;
#define ARRAY_LEN_unicode_prop_Case_Ignorable_table 785

extern uint8_t *unicode_prop_Case_Ignorable_index;
#define ARRAY_LEN_unicode_prop_Case_Ignorable_index 75

extern uint8_t *unicode_prop_ID_Start_table;
#define ARRAY_LEN_unicode_prop_ID_Start_table 1146

extern uint8_t *unicode_prop_ID_Start_index;
#define ARRAY_LEN_unicode_prop_ID_Start_index 108

extern uint8_t *unicode_prop_ID_Continue1_table;
#define ARRAY_LEN_unicode_prop_ID_Continue1_table 708

extern uint8_t *unicode_prop_ID_Continue1_index;
#define ARRAY_LEN_unicode_prop_ID_Continue1_index 66

extern uint8_t *unicode_cc_table;
#define ARRAY_LEN_unicode_cc_table 937

extern uint8_t *unicode_cc_index;
#define ARRAY_LEN_unicode_cc_index 90

extern uint32_t *unicode_decomp_table1;
#define ARRAY_LEN_unicode_decomp_table1 709

extern uint16_t *unicode_decomp_table2;
#define ARRAY_LEN_unicode_decomp_table2 709

extern uint8_t *unicode_decomp_data;
#define ARRAY_LEN_unicode_decomp_data 9452

extern uint16_t *unicode_comp_table;
#define ARRAY_LEN_unicode_comp_table 965

/* 字符串表，长度已知 */
// extern char *unicode_gc_name_table;
// #define ARRAY_LEN_unicode_gc_name_table 658   /* 计算值，包含所有终止空字符 */

extern uint8_t *unicode_gc_table;
#define ARRAY_LEN_unicode_gc_table 4122

/* 以下两个字符串表长度暂不精确，可留为 0（若使用需另外提供） */
// extern char *unicode_script_name_table;
// #define ARRAY_LEN_unicode_script_name_table 0   /* FIXME: 需实际长度 */

extern uint8_t *unicode_script_table;
#define ARRAY_LEN_unicode_script_table 2818

extern uint8_t *unicode_script_ext_table;
#define ARRAY_LEN_unicode_script_ext_table 1278

/* ------ 各种属性表 ------ */
extern uint8_t *unicode_prop_Hyphen_table;
#define ARRAY_LEN_unicode_prop_Hyphen_table 28

extern uint8_t *unicode_prop_Other_Math_table;
#define ARRAY_LEN_unicode_prop_Other_Math_table 200

extern uint8_t *unicode_prop_Other_Alphabetic_table;
#define ARRAY_LEN_unicode_prop_Other_Alphabetic_table 452

extern uint8_t *unicode_prop_Other_Lowercase_table;
#define ARRAY_LEN_unicode_prop_Other_Lowercase_table 68

extern uint8_t *unicode_prop_Other_Uppercase_table;
#define ARRAY_LEN_unicode_prop_Other_Uppercase_table 15

extern uint8_t *unicode_prop_Other_Grapheme_Extend_table;
#define ARRAY_LEN_unicode_prop_Other_Grapheme_Extend_table 112

extern uint8_t *unicode_prop_Other_Default_Ignorable_Code_Point_table;
#define ARRAY_LEN_unicode_prop_Other_Default_Ignorable_Code_Point_table 32

extern uint8_t *unicode_prop_Other_ID_Start_table;
#define ARRAY_LEN_unicode_prop_Other_ID_Start_table 11

extern uint8_t *unicode_prop_Other_ID_Continue_table;
#define ARRAY_LEN_unicode_prop_Other_ID_Continue_table 22

extern uint8_t *unicode_prop_Prepended_Concatenation_Mark_table;
#define ARRAY_LEN_unicode_prop_Prepended_Concatenation_Mark_table 19

extern uint8_t *unicode_prop_XID_Start1_table;
#define ARRAY_LEN_unicode_prop_XID_Start1_table 31

extern uint8_t *unicode_prop_XID_Continue1_table;
#define ARRAY_LEN_unicode_prop_XID_Continue1_table 23

extern uint8_t *unicode_prop_Changes_When_Titlecased1_table;
#define ARRAY_LEN_unicode_prop_Changes_When_Titlecased1_table 22

extern uint8_t *unicode_prop_Changes_When_Casefolded1_table;
#define ARRAY_LEN_unicode_prop_Changes_When_Casefolded1_table 29

extern uint8_t *unicode_prop_Changes_When_NFKC_Casefolded1_table;
#define ARRAY_LEN_unicode_prop_Changes_When_NFKC_Casefolded1_table 449

extern uint8_t *unicode_prop_Basic_Emoji1_table;
#define ARRAY_LEN_unicode_prop_Basic_Emoji1_table 144

extern uint8_t *unicode_prop_Basic_Emoji2_table;
#define ARRAY_LEN_unicode_prop_Basic_Emoji2_table 183

extern uint8_t *unicode_prop_RGI_Emoji_Modifier_Sequence_table;
#define ARRAY_LEN_unicode_prop_RGI_Emoji_Modifier_Sequence_table 72

extern uint8_t *unicode_prop_RGI_Emoji_Flag_Sequence_table;
#define ARRAY_LEN_unicode_prop_RGI_Emoji_Flag_Sequence_table 128

extern uint8_t *unicode_prop_Emoji_Keycap_Sequence_table;
#define ARRAY_LEN_unicode_prop_Emoji_Keycap_Sequence_table 4

extern uint8_t *unicode_prop_ASCII_Hex_Digit_table;
#define ARRAY_LEN_unicode_prop_ASCII_Hex_Digit_table 5

extern uint8_t *unicode_prop_Bidi_Control_table;
#define ARRAY_LEN_unicode_prop_Bidi_Control_table 10

extern uint8_t *unicode_prop_Dash_table;
#define ARRAY_LEN_unicode_prop_Dash_table 58

extern uint8_t *unicode_prop_Deprecated_table;
#define ARRAY_LEN_unicode_prop_Deprecated_table 23

extern uint8_t *unicode_prop_Diacritic_table;
#define ARRAY_LEN_unicode_prop_Diacritic_table 447

extern uint8_t *unicode_prop_Extender_table;
#define ARRAY_LEN_unicode_prop_Extender_table 116

extern uint8_t *unicode_prop_Hex_Digit_table;
#define ARRAY_LEN_unicode_prop_Hex_Digit_table 12

extern uint8_t *unicode_prop_IDS_Unary_Operator_table;
#define ARRAY_LEN_unicode_prop_IDS_Unary_Operator_table 4

extern uint8_t *unicode_prop_IDS_Binary_Operator_table;
#define ARRAY_LEN_unicode_prop_IDS_Binary_Operator_table 8

extern uint8_t *unicode_prop_IDS_Trinary_Operator_table;
#define ARRAY_LEN_unicode_prop_IDS_Trinary_Operator_table 4

extern uint8_t *unicode_prop_Ideographic_table;
#define ARRAY_LEN_unicode_prop_Ideographic_table 71

extern uint8_t *unicode_prop_Join_Control_table;
#define ARRAY_LEN_unicode_prop_Join_Control_table 4

extern uint8_t *unicode_prop_Logical_Order_Exception_table;
#define ARRAY_LEN_unicode_prop_Logical_Order_Exception_table 15

extern uint8_t *unicode_prop_Modifier_Combining_Mark_table;
#define ARRAY_LEN_unicode_prop_Modifier_Combining_Mark_table 16

extern uint8_t *unicode_prop_Noncharacter_Code_Point_table;
#define ARRAY_LEN_unicode_prop_Noncharacter_Code_Point_table 71

extern uint8_t *unicode_prop_Pattern_Syntax_table;
#define ARRAY_LEN_unicode_prop_Pattern_Syntax_table 58

extern uint8_t *unicode_prop_Pattern_White_Space_table;
#define ARRAY_LEN_unicode_prop_Pattern_White_Space_table 11

extern uint8_t *unicode_prop_Quotation_Mark_table;
#define ARRAY_LEN_unicode_prop_Quotation_Mark_table 31

extern uint8_t *unicode_prop_Radical_table;
#define ARRAY_LEN_unicode_prop_Radical_table 9

extern uint8_t *unicode_prop_Regional_Indicator_table;
#define ARRAY_LEN_unicode_prop_Regional_Indicator_table 4

extern uint8_t *unicode_prop_Sentence_Terminal_table;
#define ARRAY_LEN_unicode_prop_Sentence_Terminal_table 213

extern uint8_t *unicode_prop_Soft_Dotted_table;
#define ARRAY_LEN_unicode_prop_Soft_Dotted_table 79

extern uint8_t *unicode_prop_Terminal_Punctuation_table;
#define ARRAY_LEN_unicode_prop_Terminal_Punctuation_table 264

extern uint8_t *unicode_prop_Unified_Ideograph_table;
#define ARRAY_LEN_unicode_prop_Unified_Ideograph_table 46

extern uint8_t *unicode_prop_Variation_Selector_table;
#define ARRAY_LEN_unicode_prop_Variation_Selector_table 13

extern uint8_t *unicode_prop_White_Space_table;
#define ARRAY_LEN_unicode_prop_White_Space_table 22

extern uint8_t *unicode_prop_Bidi_Mirrored_table;
#define ARRAY_LEN_unicode_prop_Bidi_Mirrored_table 173

extern uint8_t *unicode_prop_Emoji_table;
#define ARRAY_LEN_unicode_prop_Emoji_table 239

extern uint8_t *unicode_prop_Emoji_Component_table;
#define ARRAY_LEN_unicode_prop_Emoji_Component_table 28

extern uint8_t *unicode_prop_Emoji_Modifier_table;
#define ARRAY_LEN_unicode_prop_Emoji_Modifier_table 4

extern uint8_t *unicode_prop_Emoji_Modifier_Base_table;
#define ARRAY_LEN_unicode_prop_Emoji_Modifier_Base_table 71

extern uint8_t *unicode_prop_Emoji_Presentation_table;
#define ARRAY_LEN_unicode_prop_Emoji_Presentation_table 145

extern uint8_t *unicode_prop_Extended_Pictographic_table;
#define ARRAY_LEN_unicode_prop_Extended_Pictographic_table 254

extern uint8_t *unicode_prop_Default_Ignorable_Code_Point_table;
#define ARRAY_LEN_unicode_prop_Default_Ignorable_Code_Point_table 51

/* ------ 组合表（指针数组） ------ */
extern uint8_t **unicode_prop_table;
#define ARRAY_LEN_unicode_prop_table UNICODE_PROP_COUNT   /* 枚举值 */

extern uint16_t *unicode_prop_len_table;
#define ARRAY_LEN_unicode_prop_len_table UNICODE_PROP_COUNT

/* 属性名称表（字符串） */
// extern char *unicode_prop_name_table;
// #define ARRAY_LEN_unicode_prop_name_table 0   /* FIXME: 需实际长度 */

/* 序列属性名称表（字符串） */
// extern char *unicode_sequence_prop_name_table;
// #define ARRAY_LEN_unicode_sequence_prop_name_table 0   /* FIXME: 需实际长度 */

/* RGI 序列表 */
extern uint8_t *unicode_rgi_emoji_tag_sequence;
#define ARRAY_LEN_unicode_rgi_emoji_tag_sequence 18

extern uint8_t *unicode_rgi_emoji_zwj_sequence;
#define ARRAY_LEN_unicode_rgi_emoji_zwj_sequence 2392

/* 字符串表指针（可写，堆分配） */
extern char *unicode_script_name_table;
extern char *unicode_gc_name_table;
extern char *unicode_prop_name_table;
extern char *unicode_sequence_prop_name_table;

/* 精确长度（包含末尾空字符） */
#define ARRAY_LEN_unicode_gc_name_table           526
#define ARRAY_LEN_unicode_script_name_table       2472
#define ARRAY_LEN_unicode_prop_name_table         1412
#define ARRAY_LEN_unicode_sequence_prop_name_table 126


/* ========================================================================= */
/* 枚举定义（这些定义原本就在头文件中，保持不变）                            */
/* ========================================================================= */

typedef enum {
    UNICODE_GC_Cn,
    UNICODE_GC_Lu,
    UNICODE_GC_Ll,
    UNICODE_GC_Lt,
    UNICODE_GC_Lm,
    UNICODE_GC_Lo,
    UNICODE_GC_Mn,
    UNICODE_GC_Mc,
    UNICODE_GC_Me,
    UNICODE_GC_Nd,
    UNICODE_GC_Nl,
    UNICODE_GC_No,
    UNICODE_GC_Sm,
    UNICODE_GC_Sc,
    UNICODE_GC_Sk,
    UNICODE_GC_So,
    UNICODE_GC_Pc,
    UNICODE_GC_Pd,
    UNICODE_GC_Ps,
    UNICODE_GC_Pe,
    UNICODE_GC_Pi,
    UNICODE_GC_Pf,
    UNICODE_GC_Po,
    UNICODE_GC_Zs,
    UNICODE_GC_Zl,
    UNICODE_GC_Zp,
    UNICODE_GC_Cc,
    UNICODE_GC_Cf,
    UNICODE_GC_Cs,
    UNICODE_GC_Co,
    UNICODE_GC_LC,
    UNICODE_GC_L,
    UNICODE_GC_M,
    UNICODE_GC_N,
    UNICODE_GC_S,
    UNICODE_GC_P,
    UNICODE_GC_Z,
    UNICODE_GC_C,
    UNICODE_GC_COUNT,
} UnicodeGCEnum;

typedef enum {
    UNICODE_SCRIPT_Unknown,
    UNICODE_SCRIPT_Adlam,
    UNICODE_SCRIPT_Ahom,
    UNICODE_SCRIPT_Anatolian_Hieroglyphs,
    UNICODE_SCRIPT_Arabic,
    UNICODE_SCRIPT_Armenian,
    UNICODE_SCRIPT_Avestan,
    UNICODE_SCRIPT_Balinese,
    UNICODE_SCRIPT_Bamum,
    UNICODE_SCRIPT_Bassa_Vah,
    UNICODE_SCRIPT_Batak,
    UNICODE_SCRIPT_Beria_Erfe,
    UNICODE_SCRIPT_Bengali,
    UNICODE_SCRIPT_Bhaiksuki,
    UNICODE_SCRIPT_Bopomofo,
    UNICODE_SCRIPT_Brahmi,
    UNICODE_SCRIPT_Braille,
    UNICODE_SCRIPT_Buginese,
    UNICODE_SCRIPT_Buhid,
    UNICODE_SCRIPT_Canadian_Aboriginal,
    UNICODE_SCRIPT_Carian,
    UNICODE_SCRIPT_Caucasian_Albanian,
    UNICODE_SCRIPT_Chakma,
    UNICODE_SCRIPT_Cham,
    UNICODE_SCRIPT_Cherokee,
    UNICODE_SCRIPT_Chorasmian,
    UNICODE_SCRIPT_Common,
    UNICODE_SCRIPT_Coptic,
    UNICODE_SCRIPT_Cuneiform,
    UNICODE_SCRIPT_Cypriot,
    UNICODE_SCRIPT_Cyrillic,
    UNICODE_SCRIPT_Cypro_Minoan,
    UNICODE_SCRIPT_Deseret,
    UNICODE_SCRIPT_Devanagari,
    UNICODE_SCRIPT_Dives_Akuru,
    UNICODE_SCRIPT_Dogra,
    UNICODE_SCRIPT_Duployan,
    UNICODE_SCRIPT_Egyptian_Hieroglyphs,
    UNICODE_SCRIPT_Elbasan,
    UNICODE_SCRIPT_Elymaic,
    UNICODE_SCRIPT_Ethiopic,
    UNICODE_SCRIPT_Garay,
    UNICODE_SCRIPT_Georgian,
    UNICODE_SCRIPT_Glagolitic,
    UNICODE_SCRIPT_Gothic,
    UNICODE_SCRIPT_Grantha,
    UNICODE_SCRIPT_Greek,
    UNICODE_SCRIPT_Gujarati,
    UNICODE_SCRIPT_Gunjala_Gondi,
    UNICODE_SCRIPT_Gurmukhi,
    UNICODE_SCRIPT_Gurung_Khema,
    UNICODE_SCRIPT_Han,
    UNICODE_SCRIPT_Hangul,
    UNICODE_SCRIPT_Hanifi_Rohingya,
    UNICODE_SCRIPT_Hanunoo,
    UNICODE_SCRIPT_Hatran,
    UNICODE_SCRIPT_Hebrew,
    UNICODE_SCRIPT_Hiragana,
    UNICODE_SCRIPT_Imperial_Aramaic,
    UNICODE_SCRIPT_Inherited,
    UNICODE_SCRIPT_Inscriptional_Pahlavi,
    UNICODE_SCRIPT_Inscriptional_Parthian,
    UNICODE_SCRIPT_Javanese,
    UNICODE_SCRIPT_Kaithi,
    UNICODE_SCRIPT_Kannada,
    UNICODE_SCRIPT_Katakana,
    UNICODE_SCRIPT_Katakana_Or_Hiragana,
    UNICODE_SCRIPT_Kawi,
    UNICODE_SCRIPT_Kayah_Li,
    UNICODE_SCRIPT_Kharoshthi,
    UNICODE_SCRIPT_Khmer,
    UNICODE_SCRIPT_Khojki,
    UNICODE_SCRIPT_Khitan_Small_Script,
    UNICODE_SCRIPT_Khudawadi,
    UNICODE_SCRIPT_Kirat_Rai,
    UNICODE_SCRIPT_Lao,
    UNICODE_SCRIPT_Latin,
    UNICODE_SCRIPT_Lepcha,
    UNICODE_SCRIPT_Limbu,
    UNICODE_SCRIPT_Linear_A,
    UNICODE_SCRIPT_Linear_B,
    UNICODE_SCRIPT_Lisu,
    UNICODE_SCRIPT_Lycian,
    UNICODE_SCRIPT_Lydian,
    UNICODE_SCRIPT_Makasar,
    UNICODE_SCRIPT_Mahajani,
    UNICODE_SCRIPT_Malayalam,
    UNICODE_SCRIPT_Mandaic,
    UNICODE_SCRIPT_Manichaean,
    UNICODE_SCRIPT_Marchen,
    UNICODE_SCRIPT_Masaram_Gondi,
    UNICODE_SCRIPT_Medefaidrin,
    UNICODE_SCRIPT_Meetei_Mayek,
    UNICODE_SCRIPT_Mende_Kikakui,
    UNICODE_SCRIPT_Meroitic_Cursive,
    UNICODE_SCRIPT_Meroitic_Hieroglyphs,
    UNICODE_SCRIPT_Miao,
    UNICODE_SCRIPT_Modi,
    UNICODE_SCRIPT_Mongolian,
    UNICODE_SCRIPT_Mro,
    UNICODE_SCRIPT_Multani,
    UNICODE_SCRIPT_Myanmar,
    UNICODE_SCRIPT_Nabataean,
    UNICODE_SCRIPT_Nag_Mundari,
    UNICODE_SCRIPT_Nandinagari,
    UNICODE_SCRIPT_New_Tai_Lue,
    UNICODE_SCRIPT_Newa,
    UNICODE_SCRIPT_Nko,
    UNICODE_SCRIPT_Nushu,
    UNICODE_SCRIPT_Nyiakeng_Puachue_Hmong,
    UNICODE_SCRIPT_Ogham,
    UNICODE_SCRIPT_Ol_Chiki,
    UNICODE_SCRIPT_Ol_Onal,
    UNICODE_SCRIPT_Old_Hungarian,
    UNICODE_SCRIPT_Old_Italic,
    UNICODE_SCRIPT_Old_North_Arabian,
    UNICODE_SCRIPT_Old_Permic,
    UNICODE_SCRIPT_Old_Persian,
    UNICODE_SCRIPT_Old_Sogdian,
    UNICODE_SCRIPT_Old_South_Arabian,
    UNICODE_SCRIPT_Old_Turkic,
    UNICODE_SCRIPT_Old_Uyghur,
    UNICODE_SCRIPT_Oriya,
    UNICODE_SCRIPT_Osage,
    UNICODE_SCRIPT_Osmanya,
    UNICODE_SCRIPT_Pahawh_Hmong,
    UNICODE_SCRIPT_Palmyrene,
    UNICODE_SCRIPT_Pau_Cin_Hau,
    UNICODE_SCRIPT_Phags_Pa,
    UNICODE_SCRIPT_Phoenician,
    UNICODE_SCRIPT_Psalter_Pahlavi,
    UNICODE_SCRIPT_Rejang,
    UNICODE_SCRIPT_Runic,
    UNICODE_SCRIPT_Samaritan,
    UNICODE_SCRIPT_Saurashtra,
    UNICODE_SCRIPT_Sharada,
    UNICODE_SCRIPT_Shavian,
    UNICODE_SCRIPT_Siddham,
    UNICODE_SCRIPT_Sidetic,
    UNICODE_SCRIPT_SignWriting,
    UNICODE_SCRIPT_Sinhala,
    UNICODE_SCRIPT_Sogdian,
    UNICODE_SCRIPT_Sora_Sompeng,
    UNICODE_SCRIPT_Soyombo,
    UNICODE_SCRIPT_Sundanese,
    UNICODE_SCRIPT_Sunuwar,
    UNICODE_SCRIPT_Syloti_Nagri,
    UNICODE_SCRIPT_Syriac,
    UNICODE_SCRIPT_Tagalog,
    UNICODE_SCRIPT_Tagbanwa,
    UNICODE_SCRIPT_Tai_Le,
    UNICODE_SCRIPT_Tai_Tham,
    UNICODE_SCRIPT_Tai_Viet,
    UNICODE_SCRIPT_Tai_Yo,
    UNICODE_SCRIPT_Takri,
    UNICODE_SCRIPT_Tamil,
    UNICODE_SCRIPT_Tangut,
    UNICODE_SCRIPT_Telugu,
    UNICODE_SCRIPT_Thaana,
    UNICODE_SCRIPT_Thai,
    UNICODE_SCRIPT_Tibetan,
    UNICODE_SCRIPT_Tifinagh,
    UNICODE_SCRIPT_Tirhuta,
    UNICODE_SCRIPT_Tangsa,
    UNICODE_SCRIPT_Todhri,
    UNICODE_SCRIPT_Tolong_Siki,
    UNICODE_SCRIPT_Toto,
    UNICODE_SCRIPT_Tulu_Tigalari,
    UNICODE_SCRIPT_Ugaritic,
    UNICODE_SCRIPT_Vai,
    UNICODE_SCRIPT_Vithkuqi,
    UNICODE_SCRIPT_Wancho,
    UNICODE_SCRIPT_Warang_Citi,
    UNICODE_SCRIPT_Yezidi,
    UNICODE_SCRIPT_Yi,
    UNICODE_SCRIPT_Zanabazar_Square,
    UNICODE_SCRIPT_COUNT,
} UnicodeScriptEnum;

typedef enum {
    UNICODE_PROP_Hyphen,
    UNICODE_PROP_Other_Math,
    UNICODE_PROP_Other_Alphabetic,
    UNICODE_PROP_Other_Lowercase,
    UNICODE_PROP_Other_Uppercase,
    UNICODE_PROP_Other_Grapheme_Extend,
    UNICODE_PROP_Other_Default_Ignorable_Code_Point,
    UNICODE_PROP_Other_ID_Start,
    UNICODE_PROP_Other_ID_Continue,
    UNICODE_PROP_Prepended_Concatenation_Mark,
    UNICODE_PROP_ID_Continue1,
    UNICODE_PROP_XID_Start1,
    UNICODE_PROP_XID_Continue1,
    UNICODE_PROP_Changes_When_Titlecased1,
    UNICODE_PROP_Changes_When_Casefolded1,
    UNICODE_PROP_Changes_When_NFKC_Casefolded1,
    UNICODE_PROP_Basic_Emoji1,
    UNICODE_PROP_Basic_Emoji2,
    UNICODE_PROP_RGI_Emoji_Modifier_Sequence,
    UNICODE_PROP_RGI_Emoji_Flag_Sequence,
    UNICODE_PROP_Emoji_Keycap_Sequence,
    UNICODE_PROP_ASCII_Hex_Digit,
    UNICODE_PROP_Bidi_Control,
    UNICODE_PROP_Dash,
    UNICODE_PROP_Deprecated,
    UNICODE_PROP_Diacritic,
    UNICODE_PROP_Extender,
    UNICODE_PROP_Hex_Digit,
    UNICODE_PROP_IDS_Unary_Operator,
    UNICODE_PROP_IDS_Binary_Operator,
    UNICODE_PROP_IDS_Trinary_Operator,
    UNICODE_PROP_Ideographic,
    UNICODE_PROP_Join_Control,
    UNICODE_PROP_Logical_Order_Exception,
    UNICODE_PROP_Modifier_Combining_Mark,
    UNICODE_PROP_Noncharacter_Code_Point,
    UNICODE_PROP_Pattern_Syntax,
    UNICODE_PROP_Pattern_White_Space,
    UNICODE_PROP_Quotation_Mark,
    UNICODE_PROP_Radical,
    UNICODE_PROP_Regional_Indicator,
    UNICODE_PROP_Sentence_Terminal,
    UNICODE_PROP_Soft_Dotted,
    UNICODE_PROP_Terminal_Punctuation,
    UNICODE_PROP_Unified_Ideograph,
    UNICODE_PROP_Variation_Selector,
    UNICODE_PROP_White_Space,
    UNICODE_PROP_Bidi_Mirrored,
    UNICODE_PROP_Emoji,
    UNICODE_PROP_Emoji_Component,
    UNICODE_PROP_Emoji_Modifier,
    UNICODE_PROP_Emoji_Modifier_Base,
    UNICODE_PROP_Emoji_Presentation,
    UNICODE_PROP_Extended_Pictographic,
    UNICODE_PROP_Default_Ignorable_Code_Point,
    UNICODE_PROP_ID_Start,
    UNICODE_PROP_Case_Ignorable,
    UNICODE_PROP_ASCII,
    UNICODE_PROP_Alphabetic,
    UNICODE_PROP_Any,
    UNICODE_PROP_Assigned,
    UNICODE_PROP_Cased,
    UNICODE_PROP_Changes_When_Casefolded,
    UNICODE_PROP_Changes_When_Casemapped,
    UNICODE_PROP_Changes_When_Lowercased,
    UNICODE_PROP_Changes_When_NFKC_Casefolded,
    UNICODE_PROP_Changes_When_Titlecased,
    UNICODE_PROP_Changes_When_Uppercased,
    UNICODE_PROP_Grapheme_Base,
    UNICODE_PROP_Grapheme_Extend,
    UNICODE_PROP_ID_Continue,
    UNICODE_PROP_ID_Compat_Math_Start,
    UNICODE_PROP_ID_Compat_Math_Continue,
    UNICODE_PROP_InCB,
    UNICODE_PROP_Lowercase,
    UNICODE_PROP_Math,
    UNICODE_PROP_Uppercase,
    UNICODE_PROP_XID_Continue,
    UNICODE_PROP_XID_Start,
    UNICODE_PROP_Cased1,
    UNICODE_PROP_COUNT,
} UnicodePropertyEnum;

typedef enum {
    UNICODE_SEQUENCE_PROP_Basic_Emoji,
    UNICODE_SEQUENCE_PROP_Emoji_Keycap_Sequence,
    UNICODE_SEQUENCE_PROP_RGI_Emoji_Modifier_Sequence,
    UNICODE_SEQUENCE_PROP_RGI_Emoji_Flag_Sequence,
    UNICODE_SEQUENCE_PROP_RGI_Emoji_Tag_Sequence,
    UNICODE_SEQUENCE_PROP_RGI_Emoji_ZWJ_Sequence,
    UNICODE_SEQUENCE_PROP_RGI_Emoji,
    UNICODE_SEQUENCE_PROP_COUNT,
} UnicodeSequencePropertyEnum;

/* ========================================================================= */
/* 统一的数组长度获取宏                                                     */
/* 用法：ARRAY_LEN(array_name)  => 该数组的元素个数                         */
/* ========================================================================= */
#define ARRAY_LEN(arr) ARRAY_LEN_##arr

#endif /* LIBUNICODE_TABLE_H */