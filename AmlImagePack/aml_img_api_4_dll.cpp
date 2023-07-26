#include <memory>
#ifndef __FILE_NAME__
#define __FILE_NAME__   "AmlImagePack.cpp"
#endif
#include "AmlImagePack.h"

#pragma pack(push, 4)
typedef struct _hImage_s
{
    HIMAGE hImage;
    CAmlImagePack *AmlImg;
} hImage_t;
#pragma pack(pop)

#pragma pack(push, 4)
typedef struct _hImageItem_s
{
    CAmlImagePack *AmlImg;
    HIMAGEITEM hItem;
    HIMAGE hImage;
} hImageItem_t;
#pragma pack(pop)

HIMAGE __cdecl AmlImg_open(const char *ImagePath) {
    HIMAGE hImage;
    hImage_t *result;
    CAmlImagePack *AmlImg;

    AmlImg = new CAmlImagePack();
    hImage = AmlImg->AmlImg_open(ImagePath);

    if (!hImage) {
        MESSAGE_ERR("Fail in open img[%s]\n", ImagePath); //L36
        if (AmlImg) AmlImg->~CAmlImagePack();
        return 0;
    }

    result = (hImage_t *)malloc(sizeof(hImage_t));
    result->AmlImg = AmlImg;
    result->hImage = hImage;
    return result;
}

__s32 __cdecl AmlImg_check(HIMAGE hImage) {
    return ((hImage_t*)hImage)->AmlImg->AmlImg_check(((hImage_t*)hImage)->hImage);
}

__s32 __cdecl AmlImg_close(HIMAGE hImage) {
    HIMAGE hImg = ((hImage_t*)hImage)->hImage;
    CAmlImagePack *AmlImg = ((hImage_t*)hImage)->AmlImg;
    free(hImage);
    return AmlImg->AmlImg_close(hImg);
}

__u64 __cdecl AmlImg_get_size(HIMAGE hImage) {
    return ((hImage_t*)hImage)->AmlImg->AmlImg_get_size(((hImage_t*)hImage)->hImage);
}

HIMAGEITEM __cdecl AmlImg_open_item(HIMAGE hImage, const char * MainType, const char *SubType) {
    HIMAGEITEM hItem;
    hImageItem_t *result;

    hItem = ((hImage_t*)hImage)->AmlImg->AmlImg_open_item(((hImage_t*)hImage)->hImage, MainType, SubType);

    if (!hItem) {
        MESSAGE_ERR("Fail in open item[%s, %s]\n", MainType, SubType); //L98
        return 0;
    }

    result = (hImageItem_t *)malloc(sizeof(hImageItem_t));
    result->AmlImg = ((hImage_t*)hImage)->AmlImg;
    result->hImage = ((hImage_t*)hImage)->hImage; //FIX
    result->hItem = hItem;
    return result;
}

__s32 __cdecl AmlImg_close_item(HIMAGEITEM hItem) {
    ItemInfo *hItm = (ItemInfo*)(((hImageItem_t*)hItem)->hItem); //FIX
    CAmlImagePack *AmlImg = ((hImageItem_t*)hItem)->AmlImg; //FIX
    free(hItem);
    return AmlImg->AmlImg_close_item(hItm); //FIX
	//return 0;
}

__u64 __cdecl AmlImg_get_item_size(HIMAGEITEM hItem) {
    return ((hImageItem_t*)hItem)->AmlImg->AmlImg_get_item_size(((hImageItem_t*)hItem)->hItem);
}

const char *__cdecl AmlImg_get_item_type(HIMAGEITEM hItem) {
    return ((hImageItem_t*)hItem)->AmlImg->AmlImg_get_item_type(((hImageItem_t*)hItem)->hItem);
}

__u32 __cdecl AmlImg_is_verify_item(HIMAGEITEM hItem) {
    return ((hImageItem_t*)hItem)->AmlImg->AmlImg_is_verify_item(((hImageItem_t*)hItem)->hItem);
}

__u16 __cdecl AmlImg_is_backup_item(HIMAGEITEM hItem) {
    return ((hImageItem_t*)hItem)->AmlImg->AmlImg_is_backup_item(((hImageItem_t*)hItem)->hItem);
}

__s32 __cdecl AmlImg_get_backup_itemId(HIMAGEITEM hItem, __s32 *backUpItemId) {
    return ((hImageItem_t*)hItem)->AmlImg->AmlImg_get_backup_itemId(((hImageItem_t*)hItem)->hItem, backUpItemId);
}

__s32 __cdecl AmlImg_get_item_count(HIMAGE hImage, const char *MainType) {
    return ((hImage_t*)hImage)->AmlImg->AmlImg_get_item_count(((hImage_t*)hImage)->hImage, MainType);
}

__u32 __cdecl AmlImg_read_item_data(HIMAGEITEM hItem, void *buff, __u32 readSz) {
    return ((hImageItem_t*)hItem)->AmlImg->AmlImg_read_item_data(((hImageItem_t*)hItem)->hImage, ((hImageItem_t*)hItem)->hItem, buff, readSz);
}

__s32 __cdecl AmlImg_item_seek(HIMAGEITEM hItem, __u64 offset, __u32 origin) {
    return ((hImageItem_t*)hItem)->AmlImg->AmlImg_item_seek(((hImageItem_t*)hItem)->hImage, ((hImageItem_t*)hItem)->hItem, offset, origin);
}

__s32 __cdecl AmlImg_get_next_item(HIMAGE hImage, __u32 itemId, char *MainType, char *SubType, char *FileType) {
    return ((hImage_t*)hImage)->AmlImg->AmlImg_get_next_item(((hImage_t*)hImage)->hImage, itemId, MainType, SubType, FileType);
}

char ImageDecoderInit = 0;
ImageDecoderIf_t ImageDecoder;

ImageDecoderIf_t *aml_image_packer_new() {
    if (!ImageDecoderInit) {
        ImageDecoderInit = 1;
        ImageDecoder.magic = 0x1474d494c4d41; //AMLIMG\x01\x00
        ImageDecoder.open = AmlImg_open;
        ImageDecoder.check = AmlImg_check;
        ImageDecoder.close = AmlImg_close;
        ImageDecoder.get_size = AmlImg_get_size;
        ImageDecoder.open_item = AmlImg_open_item;
        ImageDecoder.close_item = AmlImg_close_item;
        ImageDecoder.get_item_size = AmlImg_get_item_size;
        ImageDecoder.get_item_type = AmlImg_get_item_type;
        ImageDecoder.is_verify_item = AmlImg_is_verify_item;
        ImageDecoder.is_backup_item = AmlImg_is_backup_item;
        ImageDecoder.get_backup_itemId = AmlImg_get_backup_itemId;
        ImageDecoder.get_item_count = AmlImg_get_item_count;
        ImageDecoder.read_item_data = AmlImg_read_item_data;
        ImageDecoder.item_seek = AmlImg_item_seek;
        ImageDecoder.get_next_item = AmlImg_get_next_item;
        ImageDecoder.size = sizeof(ImageDecoder); //0x4c
    }
    return &ImageDecoder;
}
