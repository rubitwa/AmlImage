#ifndef __AMLIMAGE_IF_H__
#define __AMLIMAGE_IF_H__

#include <sys/types.h>

#define IMAGE_MAGIC         0x27b51956	 /* Image Magic Number		*/

#define AML_FRMWRM_VER_V1   0x01
#define AML_FRMWRM_VER_V2   0x02

#define ITEM_NAME_LEN_V1    0x20
#define ITEM_NAME_LEN_V2    0x100
#define ITEM_VERIFY_LEN     0x30
#define ITEM_ALGIN_SIZE     0x04

#define RW_MAX_SIZE         0x10000

typedef unsigned long long  __u64;
typedef long long           __s64;
typedef unsigned int        __u32;
typedef int                 __s32;
typedef unsigned short      __u16;
typedef short               __s16;
typedef char                __s8;
typedef unsigned char       __u8;

#pragma pack(push,4)
typedef struct _AmlFirmwareItem_s
{
    __u32   itemId;
    __u32   fileType;           //image file type, sparse and normal
    __u64   curoffsetInItem;    //current offset in the item
    __u64   offsetInImage;      //item offset in the image
    __u64   itemSz;             //item size in the image
    char    itemMainType[ITEM_NAME_LEN_V1];   //item main type and sub type used to index the item
    char    itemSubType[ITEM_NAME_LEN_V1];    //item main type and sub type used to index the item
    __u32   verify;
    __u16   isBackUpItem;       //this item source file is the same as backItemId
    __u16   backUpItemId;       //if 'isBackUpItem', then this is the item id
    char    reserve[24];        //don't care fields
} ItemInfo_V1;
#pragma pack(pop)

#pragma pack(push,4)
typedef struct _AmlFirmwareItem2_s
{
    __u32   itemId;
    __u32   fileType;           //image file type, sparse and normal
    __u64   curoffsetInItem;    //current offset in the item
    __u64   offsetInImage;      //item offset in the image
    __u64   itemSz;             //item size in the image
    char    itemMainType[ITEM_NAME_LEN_V2];  //item main type and sub type used to index the item
    char    itemSubType[ITEM_NAME_LEN_V2];   //item main type and sub type used to index the item
    __u32   verify;
    __u16   isBackUpItem;       //this item source file is the same as backItemId
    __u16   backUpItemId;       //if 'isBackUpItem', then this is the item id
    char    reserve[24];        //don't care fields
} ItemInfo_V2;
#pragma pack(pop)

#pragma pack(push,4)
typedef union _AmlFirmwareItem_u
{
    struct
    {
        __u32 itemId;
        __u32 fileType;
        __u64 curoffsetInItem;
        __u64 offsetInImage;
        __u64 itemSz;
    };
    ItemInfo_V1 v1;
    ItemInfo_V2 v2;
} ItemInfo;
#pragma pack(pop)

#pragma pack(push,4)
typedef struct _AmlFirmwareImg_s
{
    __u32   crc;             //check sum of the image
    __u32   version;         //firmware version
    __u32   magic;           //magic No. to say it is Amlogic firmware image
    __u64   imageSz;         //total size of this image file
    __u32   itemAlginSize;   //align size for each item
    __u32   itemNum;         //item number in the image, each item a file
    char    reserve[36];
} AmlFirmwareImg_t;
#pragma pack(pop)

typedef void* HIMAGE;
typedef void* HIMAGEITEM;

#define IMAGE_ITEM_TYPE_NORMAL  0x0000 //0
#define IMAGE_ITEM_TYPE_SPARSE  0x00FE //254
#define IMAGE_ITEM_TYPE_UBI     0x01FE //510
#define IMAGE_ITEM_TYPE_UBIFS   0x02FE //766
#define IMAGE_ITEM_TYPE_NOT_SET 0xFFFFFEE

#pragma pack(push, 4)
typedef struct _ImageDecoder_if
{
    __u64 magic;
    __u64 size;
    HIMAGE (__cdecl * open)(const char *ImagePath); //open image
    __s32 (__cdecl * check)(HIMAGE hImage); //check image
    __s32 (__cdecl * close)(HIMAGE hImage); //close image
    __u64 (__cdecl * get_size)(HIMAGE hImage); //get image size
    HIMAGEITEM (__cdecl * open_item)(HIMAGE hImage, const char * MainType, const char *SubType); //open a item in the image, like c library fopen
    __s32 (__cdecl * close_item)(HIMAGEITEM hItem); //close a item, like c library fclose
    __u64 (__cdecl * get_item_size)(HIMAGEITEM hItem); //get item size in byte
    const char *(__cdecl * get_item_type)(HIMAGEITEM hItem); //get item format, sparse, normal...
    __u32 (__cdecl * is_verify_item)(HIMAGEITEM hItem);
    __u16 (__cdecl * is_backup_item)(HIMAGEITEM hItem);
    __s32 (__cdecl * get_backup_itemId)(HIMAGEITEM hItem, __s32 *backUpItemId);
    __s32 (__cdecl * get_item_count)(HIMAGE hImage, const char *MainType); //get item count of specify main type
    __u32 (__cdecl * read_item_data)(HIMAGEITEM hItem, void *buff, __u32 readSz); //read item data, like c library fread
    __s32 (__cdecl * item_seek)(HIMAGEITEM hItem, __u64 offset, __u32 origin); //seek the item read pointer, like c library fseek
    __s32 (__cdecl * get_next_item)(HIMAGE hImage, __u32 itemId, char *MainType, char *SubType, char *FileType);
} ImageDecoderIf_t;
#pragma pack(pop)

typedef ImageDecoderIf_t *(__cdecl * aml_image_packer_new_f)();

#ifdef BUILD_DLL
    #define DLL_EXPORT __declspec(dllexport)
#else
    #define DLL_EXPORT __declspec(dllimport)
#endif

extern "C" {
    DLL_EXPORT ImageDecoderIf_t *aml_image_packer_new();
}
#endif//ifndef __AMLIMAGE_IF_H__
