#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "sparse_format.h"
#ifndef __FILE_NAME__
#define __FILE_NAME__   "AmlImagePack.cpp"
#endif
#include "AmlImagePack.h"

#define CONFIG_LINE "file=\"%s.%s\"\t\tmain_type=\"%s\"\t\tsub_type=\"%s\"\tfile_type=\"%s\"\r\n"

#define TAG_NORMALLIST  "[LIST_NORMAL]"
#define TAG_VERIFYLIST  "[LIST_VERIFY]"

#define TAG_LINE_WIN    "\r\n"
#define TAG_LINE_UNIX   "\n"
#define TAG_LINE_MACOS  "\r"

#define FILE_TAG        "file="
#define MTYPE_TAG       "main_type="
#define STYPE_TAG       "sub_type="
#define FTYPE_TAG       "file_type="

extern "C" {
	int gen_sha1sum_verify(const char* srFile, char* verifyData);
}

void FileList_free(FileList *lst) {
    FileList *ptr;
    while ((ptr = lst)) {
        lst = lst->next;
        free(ptr);
    }
}

__s32 get_filetype(__u32 fileType, char *buff) {
    if (buff) {
        switch (fileType) {
            case IMAGE_ITEM_TYPE_NORMAL:
                strcpy(buff, "normal");
                return 0;
            case IMAGE_ITEM_TYPE_SPARSE:
                strcpy(buff, "sparse");
                return 0;
            case IMAGE_ITEM_TYPE_UBI:
                strcpy(buff, "ubi");
                return 0;
            case IMAGE_ITEM_TYPE_UBIFS:
                strcpy(buff, "ubifs");
                return 0;
        }
    }
    return __LINE__; //L37
}

int set_filetype(const char *sFileType, __u32 *fileType) {
    if (!strcmp(sFileType, "ubi")) {
        *fileType = IMAGE_ITEM_TYPE_UBI;
    } else if (!strcmp(sFileType, "ubifs")) {
        *fileType = IMAGE_ITEM_TYPE_UBIFS;
    } else if (!strcmp(sFileType, "sparse")) {
        *fileType = IMAGE_ITEM_TYPE_SPARSE;
    } else if (!strcmp(sFileType, "normal")) {
        *fileType = IMAGE_ITEM_TYPE_NORMAL;
    } else {
        MESSAGE_ERR("error file_type(%s)\n", sFileType); //L54
        return __LINE__;
    }
    return 0;
}

CAmlImagePack::CAmlImagePack(void) {
    this->mapItemSorting = -1;
    this->mapItemCount = 0;
    this->header = 0;
    this->fp = 0;
    this->version = AML_FRMWRM_VER_V1;
    memset(this->errmsg, 0, sizeof(this->errmsg));
    memset(this->crc_table, 0, sizeof(this->crc_table));
}

CAmlImagePack::~CAmlImagePack(void) {
    this->AmlImg_clear();

    if ( this->header ) {
        free(this->header);
        this->header = 0;
    }
}

void CAmlImagePack::init_crc_table(void) {
    __u32 c;
    __u32 i, j;

    for (i = 0; i < 256; i++) {
        c = (__u32)i;
        for (j = 0; j < 8; j++) {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        this->crc_table[i] = c;
    }
}

__u32 CAmlImagePack::crc32(__u32 crc, unsigned char *buffer, __u32 size) {
    __u32 i;
    for (i = 0; i < size; i++) {
        crc = crc_table[(crc ^ buffer[i]) & 0xff] ^ (crc >> 8);
    }
    return crc;
}

__u32 CAmlImagePack::calc_img_crc(FILE *fp, off_t offset) {
    size_t nread;
    unsigned char buf[0x1000];
    __u32 crc = 0xffffffff;

    memset(this->crc_table, 0, 0x400);
    this->init_crc_table();
    _fseeki64(fp, offset, SEEK_SET);

    memset(buf, 0, 0x1000);
    while((nread = fread(buf, 1, 0x1000, fp)) > 0)
        crc = this->crc32(crc, buf, nread);

    return crc;
}

HIMAGE CAmlImagePack::AmlImg_open(const char *ImagePath) {
    FILE *fp;
    size_t ElementSize;

    if (!this->fp) {
        if (fopen_s(&fp, ImagePath, "rb")) {
            MESSAGE_ERR("Image open error! Open file %s failed\n", ImagePath); //L128
            return 0;
        }

        this->header = (AmlFirmwareImg_t*)malloc(sizeof(AmlFirmwareImg_t));
        if (!this->header) {
            MESSAGE_ERR("Image open error!Allocate memory for IMG_HEAD failed!\n"); //L135
            fclose(fp);
            return 0;
        }

        _fseeki64(fp, 0, SEEK_SET);
        memset(this->header, 0, sizeof(AmlFirmwareImg_t));
        if (fread(this->header, 0x40, 1, fp) != 1) {
            MESSAGE_ERR("Image open error! Read IMG_HEAD failed [%s]\n", strerror(errno)); //L144
            free(this->header);
            this->header = 0;
            fclose(fp);
            return 0;
        }

        this->version = this->header->version;
        MESSAGE("Image package version 0x%x\n", this->version);

        if (this->header->itemNum) {
            if (this->version == AML_FRMWRM_VER_V1 || this->version == AML_FRMWRM_VER_V2) {
                ElementSize = (this->version == AML_FRMWRM_VER_V1 ? sizeof(ItemInfo_V1) : sizeof(ItemInfo_V2));
                this->itemheads = (ItemInfo*)malloc(ElementSize * this->header->itemNum);
                if (!this->itemheads) {
                    MESSAGE_ERR("Image open error! Allocate memory for ITEMINFO failed\n"); //L161 L192
                    free(this->header);
                    this->header = 0;
                    fclose(fp);
                    return 0;
                }
                memset(this->itemheads, 0, ElementSize * this->header->itemNum);
                if (fread(this->itemheads, ElementSize, this->header->itemNum, fp) != this->header->itemNum) {
                    this->AmlImg_clear();
                    MESSAGE_ERR("Image open errror! Read ITEMINFO failed [%s]\n", strerror(errno)); //L172 L203
                    free(this->header);
                    this->header = 0;
                    fclose(fp);
                    return 0;
                }
            }
        }
        this->fp = fp;
    }
    return this->fp;
}

__s32 CAmlImagePack::AmlImg_check(HIMAGE hImage) {
    memset(this->errmsg, 0, sizeof(this->errmsg));
    if (this->header->magic != IMAGE_MAGIC) {
        MESSAGE_ERR("Image check error! The magic number is not match\n"); //L260
        return -1;
    }
    //if (this->calc_img_crc(this->fp, ITEM_ALGIN_SIZE) != this->header->crc) {
    if (this->calc_img_crc(this->fp, this->header->itemAlginSize) != this->header->crc) {
        MESSAGE_ERR("Image check error! CRC check failed!\n"); //L267
        return -1;
    }
    return 0;
}

__s32 CAmlImagePack::AmlImg_check(const char *ImagePath) {
    FILE *fp;
    AmlFirmwareImg_t header;

    fp = fopen(ImagePath, "rb");
    if (!fp) {
        MESSAGE_ERR("Image check error! Open file %s failed\n", ImagePath); //L283
        return -1;
    }

    memset(&header, 0, sizeof(header));
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        MESSAGE_ERR("Image check error! Read img head failed [%s]\n", strerror(errno)); //L291
        fclose(fp);
        return -1;
    }

    if (header.magic != IMAGE_MAGIC) {
        MESSAGE_ERR("Image check error! The magic number is not match\n"); //L298
        fclose(fp);
        return -1;
    }
    //if (this->calc_img_crc(fp, ITEM_ALGIN_SIZE) != header.crc) {
    if (this->calc_img_crc(fp, header.itemAlginSize) != header.crc) {
        MESSAGE_ERR("Image check error! CRC check failed!\n"); //L306
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

__u64 CAmlImagePack::AmlImg_get_size(HIMAGE hImage) {
    if (this->header)
        return this->header->imageSz;
    return 0;
}

void CAmlImagePack::AmlImg_clear(void) {
    if (this->itemheads)
        free(this->itemheads);
}

__s32 CAmlImagePack::AmlImg_close(HIMAGE hImage) {
    __s32 result;
    this->AmlImg_clear();
    if (this->header) {
        free(this->header);
        this->header = 0;
    }
    if (!this->fp)
        return -1;
    result = fclose(this->fp);
    this->fp = 0;
    return result;
}

HIMAGEITEM CAmlImagePack::AmlImg_open_item(HIMAGE hImage, const char * MainType, const char *SubType) {
    __u32 i;
    size_t nlen;
    ItemInfo *ptr;

    if (this->version == AML_FRMWRM_VER_V1) nlen = ITEM_NAME_LEN_V1;
    else if (this->version == AML_FRMWRM_VER_V2) nlen = ITEM_NAME_LEN_V2;
    else nlen = 0;

    if (!MainType || !SubType || strlen(MainType) > nlen || strlen(SubType) > nlen) {
        MESSAGE_ERR("Open item error! Invalid parameter!\n"); //L338
        return 0;
    }

    i = this->header->itemNum;
    ptr = this->itemheads;

    if (this->version == AML_FRMWRM_VER_V1) {
        while (ptr && i) {
            if (!strcmp(ptr->v1.itemMainType, MainType) && !strcmp(ptr->v1.itemSubType, SubType))
                return ptr;
            (*(ItemInfo_V1**)&ptr)++, i--;
        }
        MESSAGE_ERR("Open item error! Can not find specific item!\n"); //L357
    } else if (this->version == AML_FRMWRM_VER_V2) {
        while (ptr && i) {
            if (!strcmp(ptr->v2.itemMainType, MainType) && !strcmp(ptr->v2.itemSubType, SubType))
                return ptr;
            (*(ItemInfo_V2**)&ptr)++, i--;
        }
        MESSAGE_ERR("Open item error! Can not find specific item!\n"); //L376
    }
    return 0;
}

__u32 CAmlImagePack::AmlImg_read_item_data(HIMAGE hImage, HIMAGEITEM hItem, void* buff, __u32 readSz) {
    __u32 i;
    ItemInfo *ptr;

    if (!hItem || !buff) {
        MESSAGE_ERR("Read item data error! Invalid parameter!\n"); //L396
        return 0;
    }

    i = this->header->itemNum;
    ptr = this->itemheads;

    if (this->version == AML_FRMWRM_VER_V1) {
        while (ptr && i) {
            if (!strcmp(((ItemInfo*)hItem)->v1.itemMainType, ptr->v1.itemMainType) && !strcmp(((ItemInfo*)hItem)->v1.itemSubType, ptr->v1.itemSubType))
                break;
            (*(ItemInfo_V1**)&ptr)++, i--;
        }
    } else if (this->version == AML_FRMWRM_VER_V2) {
        while (ptr && i) {
            if (!strcmp(((ItemInfo*)hItem)->v2.itemMainType, ptr->v2.itemMainType) && !strcmp(((ItemInfo*)hItem)->v2.itemSubType, ptr->v2.itemSubType))
                break;
            (*(ItemInfo_V2**)&ptr)++, i--;
        }
    } else {
        return 0;
    }
    if (!ptr || !i) {
        MESSAGE_ERR("Read item data error! Invalid item handle!\n"); //L415 L453
        return 0;
    }
    _fseeki64(this->fp, ((ItemInfo*)hItem)->curoffsetInItem + ((ItemInfo*)hItem)->offsetInImage, SEEK_SET);
    if (((ItemInfo*)hItem)->curoffsetInItem + readSz >= ((ItemInfo*)hItem)->itemSz)
        readSz = ((ItemInfo*)hItem)->itemSz - ((ItemInfo*)hItem)->curoffsetInItem;
    if (fread(buff, readSz, 1, this->fp) == 1) {
		((ItemInfo*)hItem)->curoffsetInItem += readSz;
        return readSz;
    }
    MESSAGE_ERR("Read item date error! Read item buffer failed! [%s]\n", strerror(errno)); //L431 L469
    return 0;
}

__s32 CAmlImagePack::AmlImg_item_seek(HIMAGE hImage, HIMAGEITEM hItem, __s64 offset, __u32 origin) {
    __u32 i;
    ItemInfo *ptr;

    if (!hItem) {
        MESSAGE_ERR("Seek image item error! Invalid parameter!\n"); //L485
        return -1;
    }

    i = this->header->itemNum;
    ptr = this->itemheads;

    if (this->version == AML_FRMWRM_VER_V1) {
        while (ptr && i) {
            if (!strcmp(((ItemInfo*)hItem)->v1.itemMainType, ptr->v1.itemMainType) && !strcmp(((ItemInfo*)hItem)->v1.itemSubType, ptr->v1.itemSubType))
                break;
            (*(ItemInfo_V1**)&ptr)++, i--;
        }
    } else if (this->version == AML_FRMWRM_VER_V2) {
        while (ptr && i) {
            if (!strcmp(((ItemInfo*)hItem)->v2.itemMainType, ptr->v2.itemMainType) && !strcmp(((ItemInfo*)hItem)->v2.itemSubType, ptr->v2.itemSubType))
                break;
            (*(ItemInfo_V2**)&ptr)++, i--;
        }
    } else {
        return 0;
    }
    if (!ptr || !i) {
        MESSAGE_ERR("Seek image item error! Invalid item handle!\n"); //L504 L544
        return -1;
    }
    _fseeki64(this->fp, ((ItemInfo*)hItem)->offsetInImage, SEEK_SET); //ptr->offsetInImage
    if (origin == SEEK_SET) {
		((ItemInfo*)hItem)->curoffsetInItem = offset;
        return _fseeki64(this->fp, offset, SEEK_CUR);
    } else if (origin == SEEK_CUR) {
		((ItemInfo*)hItem)->curoffsetInItem += offset;
        return _fseeki64(this->fp, ((ItemInfo*)hItem)->curoffsetInItem, SEEK_CUR);
    } else if (origin == SEEK_END) {
		((ItemInfo*)hItem)->curoffsetInItem = ((ItemInfo*)hItem)->itemSz + offset;
        return _fseeki64(this->fp, ((ItemInfo*)hItem)->curoffsetInItem, SEEK_CUR);
    }
    MESSAGE_ERR("Seek image item error! Invalid origin!\n"); //L524 L564
    return -1;
}

__u64 CAmlImagePack::AmlImg_get_item_size(HIMAGEITEM hItem) {
    __u32 i;
    ItemInfo *ptr;

    if (!hItem) {
        MESSAGE_ERR("Get image item size error! Invalid parameter!\n"); //L576
        return 0;
    }

    i = this->header->itemNum;
    ptr = this->itemheads;

    if (this->version == AML_FRMWRM_VER_V1) {
        while (ptr && i) {
            if (!strcmp(((ItemInfo*)hItem)->v1.itemMainType, ptr->v1.itemMainType) && !strcmp(((ItemInfo*)hItem)->v1.itemSubType, ptr->v1.itemSubType))
                return ptr->itemSz;
            (*(ItemInfo_V1**)&ptr)++, i--;
        }
    } else if (this->version == AML_FRMWRM_VER_V2) {
        while (ptr && i) {
            if (!strcmp(((ItemInfo*)hItem)->v2.itemMainType, ptr->v2.itemMainType) && !strcmp(((ItemInfo*)hItem)->v2.itemSubType, ptr->v2.itemSubType))
                return ptr->itemSz;
            (*(ItemInfo_V2**)&ptr)++, i--;
        }
    } else {
        return 0;
    }
    if (!ptr || !i)
        MESSAGE_ERR("Get image item size error! Invalid item handle!\n"); //L595 L617
    return 0;
}

__u32 CAmlImagePack::AmlImg_is_verify_item(HIMAGEITEM hItem) {
    __u32 i;
    ItemInfo *ptr;

    if (!hItem) {
        MESSAGE_ERR("Get image item verify error! Invalid parameter!\n"); //L631
        return 2;
    }

    i = this->header->itemNum;
    ptr = this->itemheads;

    if (this->version == AML_FRMWRM_VER_V1) {
        while (ptr && i) {
            if (!strcmp(((ItemInfo*)hItem)->v1.itemMainType, ptr->v1.itemMainType) && !strcmp(((ItemInfo*)hItem)->v1.itemSubType, ptr->v1.itemSubType))
                return ptr->v1.verify;
            (*(ItemInfo_V1**)&ptr)++, i--;
        }
    } else if (this->version == AML_FRMWRM_VER_V2) {
        while (ptr && i) {
            if (!strcmp(((ItemInfo*)hItem)->v2.itemMainType, ptr->v2.itemMainType) && !strcmp(((ItemInfo*)hItem)->v2.itemSubType, ptr->v2.itemSubType))
                return ptr->v2.verify;
            (*(ItemInfo_V2**)&ptr)++, i--;
        }
    } else {
        return 2;
    }
    if (!ptr || !i) {
        MESSAGE_ERR("Get image item verify error! Invalid item handle!\n"); //L650 L672
    }
    return 2;
}

__u32 CAmlImagePack::AmlImg_is_backup_item(HIMAGEITEM hItem) {
    if (!hItem) {
        MESSAGE_ERR("Get image item backup error! Invalid parameter!\n"); //L686
        return 0;
    }
    if (this->version == AML_FRMWRM_VER_V1)
        return ((ItemInfo*)hItem)->v1.isBackUpItem;
    else if (this->version == AML_FRMWRM_VER_V2)
        return ((ItemInfo*)hItem)->v2.isBackUpItem;
    return 0;
}

__s32 CAmlImagePack::AmlImg_close_item(HIMAGEITEM hItem) {
    return 0;
}

__s32 CAmlImagePack::AmlImg_get_backup_itemId(HIMAGEITEM hItem, __s32 *backUpItemId) {
    if (!hItem) {
        MESSAGE_ERR("Get image item backup error! Invalid parameter!\n"); //L747
        return __LINE__; //L748
    }
    if (this->version == AML_FRMWRM_VER_V1)
        *backUpItemId = ((ItemInfo*)hItem)->v1.backUpItemId;
    else if (this->version == AML_FRMWRM_VER_V2)
        *backUpItemId = ((ItemInfo*)hItem)->v2.backUpItemId;
    return 0;
}

const char *CAmlImagePack::AmlImg_get_item_type(HIMAGEITEM hItem) {
    __u32 i;
    ItemInfo *ptr;
    __u32 fileType;

    if (!hItem) {
        MESSAGE_ERR("Get image item type error! Invalid parameter!\n"); //L774
        return 0;
    }

    i = this->header->itemNum;
    fileType = 0;
    ptr = this->itemheads;

    if (this->version == AML_FRMWRM_VER_V1) {
        while (ptr && i) {
            if (!strcmp(((ItemInfo*)hItem)->v1.itemMainType, ptr->v1.itemMainType) && !strcmp(((ItemInfo*)hItem)->v1.itemSubType, ptr->v1.itemSubType))
                break;
            (*(ItemInfo_V1**)&ptr)++, i--;
        }
    } else if (this->version == AML_FRMWRM_VER_V2) {
        while (ptr && i) {
            if (!strcmp(((ItemInfo*)hItem)->v2.itemMainType, ptr->v2.itemMainType) && !strcmp(((ItemInfo*)hItem)->v2.itemSubType, ptr->v2.itemSubType))
                break;
            (*(ItemInfo_V2**)&ptr)++, i--;
        }
    }
    if (!ptr || !i) {
        MESSAGE_ERR("Get image item type error! Invalid item handle!\n"); //L793 L815
    } else fileType = ptr->fileType;
    switch (fileType) {
        case IMAGE_ITEM_TYPE_UBI: return "ubi";
        case IMAGE_ITEM_TYPE_UBIFS: return "ubifs";
        case IMAGE_ITEM_TYPE_SPARSE: return "sparse";
    }
    return "normal";
}

__s32 CAmlImagePack::AmlImg_get_item_count(HIMAGE hImage, const char *MainType) {
    __u32 i, cnt;
    ItemInfo *ptr;

    i = this->header->itemNum;
    cnt = 0;
    ptr = this->itemheads;

    if (!MainType || !MainType[0]) //if (!MainType || !strlen(MainType))
        return this->header->itemNum; //return count of items in container: this->itemheads

    if (this->version == AML_FRMWRM_VER_V1) {
        if (strlen(MainType) <= ITEM_NAME_LEN_V1) {
            while (ptr && i) {
                if (!strcmp(MainType, ptr->v1.itemMainType))
                    cnt++;
                (*(ItemInfo_V1**)&ptr)++, i--;
            }
            return cnt;
        }
        MESSAGE_ERR("Get image item count error! maintype string length is large than %u\n", ITEM_NAME_LEN_V1); //L852
        return -1;
    } else if (this->version == AML_FRMWRM_VER_V2) {
        if (strlen(MainType) <= ITEM_NAME_LEN_V2) {
            while (ptr && i) {
                if (!strcmp(MainType, ptr->v2.itemMainType))
                    cnt++;
                (*(ItemInfo_V2**)&ptr)++, i--;
            }
            return cnt;
        }
        MESSAGE_ERR("Get image item count error! maintype string length is large than %u\n", ITEM_NAME_LEN_V2); //L876
        return -1;
    }
    return cnt;
}

__s32 CAmlImagePack::AmlImg_get_next_item(HIMAGE hImage, __u32 itemId, char *MainType, char *SubType, char *fileType) {
    __u32 i;
    ItemInfo *ptr;

    if (!MainType || !SubType) {
        MESSAGE_ERR("Get next item error! NULL parameter! \n"); //L904
        return -1;
    }

    i = this->header->itemNum;
    ptr = this->itemheads;

    if (this->version == AML_FRMWRM_VER_V1) {
        while (ptr && i) {
            if (ptr->itemId == itemId) {
                strcpy(MainType, ptr->v1.itemMainType);
                strcpy(SubType, ptr->v1.itemSubType);
                if (fileType) //FIX
                    get_filetype(ptr->fileType, fileType);
                return 0;
            }
            (*(ItemInfo_V1**)&ptr)++, i--;
        }
    } else if (this->version == AML_FRMWRM_VER_V2) {
        while (ptr && i) {
            if (ptr->itemId == itemId) {
                strncpy(MainType, ptr->v2.itemMainType, ITEM_NAME_LEN_V2);
                strncpy(SubType, ptr->v2.itemSubType, ITEM_NAME_LEN_V2);
                if (fileType) //FIX
                    get_filetype(ptr->fileType, fileType);
                return 0;
            }
            (*(ItemInfo_V2**)&ptr)++, i--;
        }
    }
    if (!ptr || !i) {
        MESSAGE_ERR("Open item error! Can not find specific item!\n"); //L923 L947
        return __LINE__; //L924 L948
    }
    return -1;
}

__s32 CAmlImagePack::AmlImg_pack_create_mapItem(const char *FileName, const char *MainType, const char *SubType, const char *FileType, int verify, int GenSha1Sum) {
    int cmp;
    char *ptr;
    FileList **mapItem, *file;

    file = (FileList*)malloc(sizeof(FileList));
    if (!file) {
        MESSAGE_ERR("failed to allocate MAPITEM memory\n"); //L1010
        return -1;
    }
    memset(file, 0, sizeof(FileList));

    ptr = strcpy(file->key, SubType);
    *(ptr += strlen(SubType))++ = '.';
    strcpy(ptr, MainType);

    strcpy(file->name, FileName);
    file->genSha1Sum = GenSha1Sum;
    if (this->mapItemVersion == AML_FRMWRM_VER_V1) {
        file->item.v1.verify = verify;
        strcpy(file->item.v1.itemSubType, SubType);
        strcpy(file->item.v1.itemMainType, MainType);
    } else if (this->mapItemVersion == AML_FRMWRM_VER_V2) {
        file->item.v2.verify = verify;
        strcpy(file->item.v2.itemSubType, SubType);
        strcpy(file->item.v2.itemMainType, MainType);
    }
    file->item.fileType = IMAGE_ITEM_TYPE_NOT_SET;

    if (FileType && set_filetype(FileType, &file->item.fileType) < 0) {
        MESSAGE_ERR("Fail in parse str file_type to int %s\n", FileType); //L1026
        return -1;
    }

    mapItem = &this->mapItem;
    while (*mapItem != 0) {
        cmp = strcmp((*mapItem)->key, file->key);
        if (!cmp) { // mapItem->insert(std::pair<std::string, struct FileList * {ItemInfo item; char name[256]; __s32 genSha1Sum; }>(key, file)).second == false
            MESSAGE_ERR("Cfg Item[%s] duplicated\n", file->key); //L1035
            free(file); //FIX
            return __LINE__; //L1036
        } else if ((this->mapItemSorting == -1 ? (this->mapItemVersion == AML_FRMWRM_VER_V1 ? 0 : 1) : this->mapItemSorting) && cmp > 0) { // version 1 used linked list without sorting
            file->next = *mapItem;
            break;
        }
        mapItem = &(*mapItem)->next;
    }
    *mapItem = file;
    //MESSAGE("f(%s)failed as [%s] inserted failed\n", __FUNCTION__, file->key); //L1044
    //free(file);
    //return __LINE__;
    this->mapItemCount++;
    return 0;
}

int get_tag(const char *tag, char **data, char **value) {
    char *ptr;
    if (!(ptr = strstr(*data, tag))) {
        MESSAGE_ERR("Fail to get tag[%s]\n", tag); //L1060
        return __LINE__;
    }
    if (!(ptr = strchr(&ptr[strlen(tag)], '"'))) {
        MESSAGE_ERR("Fail to left \" for tag(%s)\n", tag); //L1067
        return __LINE__;
    }
    *value = ++ptr;
    if (!(ptr = strchr(ptr, '"'))) {
        MESSAGE_ERR("Fail to right \" for tag(%s)\n", tag); //L1072
        return __LINE__;
    }
    *ptr++ = 0;
    while (*ptr == 32 || *ptr == 9) ptr++; // skip spaces and \t
    *data = ptr;
    return 0;
}

__s32 CAmlImagePack::AmlImg_cfg_parse(const char *ConfigPath, __s32 version) {
    FILE *fp;
    long fsize;
    __s32 verify, ret = 0;
    char *key_normal, *key_verify;
    char *data, *line, *token, *tag_line;
    char *file, *MainType, *SubType, *fileType;

    if (version == AML_FRMWRM_VER_V1 || version == AML_FRMWRM_VER_V2) {
        this->mapItemVersion = version;
        if (!(fp = fopen(ConfigPath, "rb"))) {
            MESSAGE_ERR("failed to open configuration file\n");//L1107
            return -1;
        }
        fseek(fp, 0, SEEK_END);
        fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (!(data = (char*)malloc(fsize + 1))) {
            MESSAGE_ERR("allocate memory failed\n");//1118
            return -1;
        }
        if (fread(data, fsize, 1, fp) != 1) {
            MESSAGE_ERR("file read error [%s] \n", strerror(errno)); //L1125
            free(data);
            fclose(fp);
            return -1;
        }
        fclose(fp);

        key_normal = strstr(data, TAG_NORMALLIST);
        if (key_normal == 0) {
            MESSAGE("image.cfg parse err: miss [LIST_NORMAL] node \n");
        }

        key_verify = strstr(data, TAG_VERIFYLIST);
        if (key_verify == 0) {
            MESSAGE("image.cfg parse: no [LIST_VERIFY] node \n");
        }

        if (!key_normal && !key_verify) {
            MESSAGE_ERR("key type should be [LIST_NORMAL] or [LIST_VERIFY]\n"); //L1147
            return -1;
        }

        tag_line = TAG_LINE_WIN;
        token = strtok(data, tag_line);
        if (token == 0) {
            tag_line = TAG_LINE_UNIX;
            token = strtok(data, tag_line);
        }

        do {
            while (*token == 32 || *token == 9) // skip spaces and \t
                ++token;

            if (*token == '#')
                continue;

            line = strstr(token, FILE_TAG);
            if (line == 0)
                continue;

            if (key_verify > key_normal) {
                if (line > key_normal && line < key_verify) {
                    verify = 0;
                } else if (line > key_verify) {
                    verify = 1;
                } else {
                    MESSAGE("invalid list\n");
                    continue;
                }
            } else {
                if (line > key_normal) {
                    verify = 0;
                } else if (line > key_verify && line < key_normal) {
                    verify = 1;
                } else {
                    MESSAGE("invalid list\n");
                    continue;
                }
            }

            if (get_tag(FILE_TAG, &line, &file)) {
                MESSAGE_ERR("Fail in get tag(%s) at line(%s)\n", "file=", line); //L1217
                break;
            }

            if (get_tag(MTYPE_TAG, &line, &MainType)) {
                MESSAGE_ERR("Fail in get tag(%s) at line(%s)\n", "main_type=", line); //L1224
                break;
            }

            if (get_tag(STYPE_TAG, &line, &SubType)) {
                MESSAGE_ERR("Fail in get tag(%s) at line(%s)\n", "subtype=", line); //L1231
                break;
            }

            if (version == AML_FRMWRM_VER_V2 && file) {
                if (get_tag(FTYPE_TAG, &line, &fileType)) {
                    MESSAGE_ERR("Fail in get tag(%s) at line(%s)\n", "file_type=", line); //L1238
                    break;
                }
            } else {
                fileType = 0;
            }

            ret = this->AmlImg_pack_create_mapItem(file, MainType, SubType, fileType, verify, 0);
            if (!ret && verify)
                ret = this->AmlImg_pack_create_mapItem(file, "VERIFY", SubType, "normal", 0, 1);

            if (ret) {
                MESSAGE_ERR("Fail in create map item, ret=%d\n", ret); //L1255
                break;
            }
        } while ((token = strtok(0, tag_line)));
    } else {
        MESSAGE_ERR("Invalid pack version %d", version); //L1265
        return -1;
    }
    if (ret && this->mapItem) {
        FileList_free(this->mapItem);
        this->mapItem = 0;
		this->mapItemCount = 0;
    }
    free(data);
    return ret;
}

__s32 CAmlImagePack::AmlImg_pack(const char *ConfigPath, const char *ImageDirectory, const char *ImagePath, __s32 version) {
    size_t szr;
    void *itemheads;
    char *block, *ptr;
    struct _stat64 FileStat;
    AmlFirmwareImg_t header;
    __u32 ElementSize, itemId;
    ItemInfo *aml_sysrecovery;
    FileList *file, *duplicate;
    char FilePath[256], fileType[260];
    FILE *fp_write, *fp_read;

    memset(FilePath, 0, sizeof(FilePath));
    ptr = FilePath;
    while ((*ptr++ = *ImageDirectory++));
    *--ptr = '\\';
    *++ptr = 0;

    if (this->AmlImg_cfg_parse(ConfigPath, version)) {
        MESSAGE_ERR("Fail in parse cfg file\n"); //L1296
        return -1;
    }

    memset(&header, 0, sizeof(header));
    header.version = version;
    header.itemNum = this->mapItemCount;

    if (!(block = (char*)malloc(RW_MAX_SIZE))) {
        MESSAGE_ERR("Allocate memory for pack failed\n"); //L1307
        return -1;
    }

    if (!(fp_write = fopen(ImagePath, "wb+"))) {
        MESSAGE_ERR("Open pack image %s failed\n", ImagePath); //L1314
        free(block);
        return -1;
    }

    if (fwrite(&header, sizeof(AmlFirmwareImg_t), 1, fp_write) != 1) {
        MESSAGE_ERR("Write image head failed [%s]\n", strerror(errno)); //L1321
        goto fail;
    }

    if (version == AML_FRMWRM_VER_V1 || version == AML_FRMWRM_VER_V2) {
        itemheads = 0;
        ElementSize = (version == AML_FRMWRM_VER_V1 ? sizeof(ItemInfo_V1) : sizeof(ItemInfo_V2));
        if (header.itemNum > 0 && !(itemheads = malloc(ElementSize * header.itemNum))) {
            MESSAGE_ERR("Allocate memory for itemheads failed\n"); //L1342
            goto fail;
        }
        if (itemheads != 0) {
            memset(itemheads, 0, ElementSize * header.itemNum);
            if (fwrite(itemheads, ElementSize, header.itemNum, fp_write) != header.itemNum) {
                MESSAGE_ERR("Write itemheads failed [%s]\n", strerror(errno)); //L1352
                free(itemheads);
                goto fail;
            }
            free(itemheads);
        }

        itemId = 0;
        aml_sysrecovery = 0;
        file = this->mapItem;

        for (; file != 0; file = file->next) {
            file->item.itemId = itemId++;
            if (version == AML_FRMWRM_VER_V2) {
                if (!strcmp("aml_sysrecovery", file->item.v2.itemSubType) && !strcmp("PARTITION", file->item.v2.itemMainType)) {
                    aml_sysrecovery = &file->item;
                    continue;
                }
                strcpy(ptr, file->name);
                MESSAGE("Pack Item[%-12s, %16s] from (%s),", file->item.v2.itemMainType, file->item.v2.itemSubType, FilePath);
                for (duplicate = this->mapItem; duplicate != 0 && duplicate != file; duplicate = duplicate->next) {
                    if (duplicate->genSha1Sum == file->genSha1Sum && !strcmp(file->name, duplicate->name)) {
                        file->item.v2.isBackUpItem = 1;
                        file->item.v2.backUpItemId = duplicate->item.v2.itemId;
                        file->item.v2.itemSz = duplicate->item.v2.itemSz;
                        file->item.v2.offsetInImage = duplicate->item.v2.offsetInImage;
                        file->item.v2.fileType = duplicate->item.v2.fileType;
                        MESSAGE_EXT("Duplicated for %s\n\n", file->name);
                        break;
                    }
                }
                if (file != duplicate)
                    continue;
            } else if (version == AML_FRMWRM_VER_V1) {
                strcpy(ptr, file->name);
                MESSAGE("Pack Item[%-12s, %16s] from (%s),", file->item.v1.itemMainType, file->item.v1.itemSubType, FilePath);
            }

            file->item.offsetInImage = _ftelli64(fp_write);
            if (!file->genSha1Sum) {
                if (!(fp_read = fopen(FilePath, "rb"))) {
                    MESSAGE_ERR("failed to open source file : %s \n", FilePath); //L1422
                    goto fail;
                }
                if (_stat64(FilePath, &FileStat)) {
                    MESSAGE_ERR("Get status information of %s failed\n", FilePath); //L1430
                    fclose(fp_read);
                    goto fail;
                }
                MESSAGE_EXT("sz[0x%llx]B,", FileStat.st_size);
                file->item.itemSz = FileStat.st_size;
                if (file->item.offsetInImage % ITEM_ALGIN_SIZE)
                    file->item.offsetInImage += ITEM_ALGIN_SIZE - file->item.offsetInImage % ITEM_ALGIN_SIZE;
                _fseeki64(fp_write, file->item.offsetInImage, SEEK_SET);
                if (file->item.itemSz) {
                    memset(block, 0, RW_MAX_SIZE);
                    szr = fread(block, 1, RW_MAX_SIZE, fp_read);
                    if (file->item.fileType == 0xFFFFFEE && szr >= RW_MAX_SIZE) {
                        if (((sparse_header_t*)block)->magic == SPARSE_HEADER_MAGIC
                            && ((sparse_header_t*)block)->major_version == SPARSE_HEADER_MAJOR_VER
                            && ((sparse_header_t*)block)->file_hdr_sz == FILE_HEAD_SIZE
                            && ((sparse_header_t*)block)->chunk_hdr_sz == CHUNK_HEAD_SIZE) {
                            file->item.fileType = IMAGE_ITEM_TYPE_SPARSE;
                        } else {
                            file->item.fileType = IMAGE_ITEM_TYPE_NORMAL;
                        }
                    }
                    MESSAGE_EXT("fileType[%s]\t", (!get_filetype(file->item.fileType, fileType) ? fileType : "unknown"));
                    while (szr > 0) {
                        if (fwrite(block, 1, szr, fp_write) != szr) {
                            MESSAGE_ERR("Write %d bytes to image file failed [%s]\n", szr, strerror(errno)); //L1482
                            fclose(fp_read);
                            goto fail;
                        }
                        memset(block, 0, RW_MAX_SIZE);
                        szr = fread(block, 1, RW_MAX_SIZE, fp_read);
                    }
                } else {
                    file->item.fileType = IMAGE_ITEM_TYPE_NORMAL;
                }
                fclose(fp_read);
            } else {
                file->item.itemSz = ITEM_VERIFY_LEN;
                memset(block, 0, RW_MAX_SIZE);
                if (gen_sha1sum_verify(FilePath, block)) {
                    MESSAGE_ERR("Gen file %s sha1sum failed!\n", FilePath); //L1505
                    goto fail;
                }
                MESSAGE_EXT("vry[%s]\t", block);
                if (fwrite(block, 1, ITEM_VERIFY_LEN, fp_write) != ITEM_VERIFY_LEN) {
                    MESSAGE_ERR("Write verify data for %s failed [%s]\n", FilePath, strerror(errno)); //L1514
                    goto fail;
                }
            }
            MESSAGE_EXT("\n");
            fflush(fp_write); //bugfix: if not flush, ftell return wrong position like: bytes writed 0x001AAADA, but ftell return 0x001AAAED
        }

        if (aml_sysrecovery != 0) {
            aml_sysrecovery->itemSz = _ftelli64(fp_write);
            aml_sysrecovery->offsetInImage = 0;
            aml_sysrecovery->fileType = IMAGE_ITEM_TYPE_NORMAL;
            MESSAGE("pack item [%-12s, %16s] item for all of this image size:%lld bytes\n", aml_sysrecovery->v2.itemMainType, aml_sysrecovery->v2.itemSubType, aml_sysrecovery->itemSz);
        }

        _fseeki64(fp_write, sizeof(header), SEEK_SET);
        for (file = this->mapItem; file; file = file->next) {
            if (fwrite(&file->item, ElementSize, 1, fp_write) != 1) {
                //MESSAGE_ERR("Write item head of %s.%s failed [%s]\n", file->item.v1.itemSubType, file->item.v1.itemMainType, strerror(errno)); //L1544
                MESSAGE_ERR("Write item head of %s failed [%s]\n", file->key, strerror(errno)); //L1544
                goto fail;
            }
        }

        header.magic = IMAGE_MAGIC;
        header.itemAlginSize = ITEM_ALGIN_SIZE;
        _fseeki64(fp_write, 0, SEEK_END);
        header.imageSz = _ftelli64(fp_write);
        _fseeki64(fp_write, 0, SEEK_SET);
        if (fwrite(&header, sizeof(header), 1, fp_write) != 1) {
            MESSAGE_ERR("Rewrite image head failed [%s]\n", strerror(errno)); //L1560
            goto fail;
        }

        header.crc = this->calc_img_crc(fp_write, ITEM_ALGIN_SIZE);
        _fseeki64(fp_write, 0, SEEK_SET);
        if (fwrite(&header.crc, sizeof(header.crc), 1, fp_write) != 1) {
        //libimagepack.dll: fwrite(&header.crc, 1, header.itemAlginSize, fp_write) != header.itemAlginSize
            MESSAGE_ERR("Write crc failed [%s]\n", strerror(errno)); //L1571
            goto fail;
        }

        MESSAGE("version:0x%x crc:0x%08x size:%lld bytes[%uMB]\n", header.version, header.crc, header.imageSz, (unsigned int)(header.imageSz >> 20));
        fclose(fp_write);
        free(block);
        //sub_10006EB0();//free_filelist(this->mapItem);
        return 0;
    }
    MESSAGE_ERR("Invalid pack version %d", version); //L1583
fail:
    fclose(fp_write);
    free(block);
    if (this->mapItem) {
        FileList_free(this->mapItem);
        this->mapItem = 0;
		this->mapItemCount = 0;
    }
    return -1;
}

__s32 CAmlImagePack::AmlImg_unpack(const char *ImagePath, const char *ImageDirectory) {
    //return this->AmlImg_unpack_addSecure(ImagePath, ImageDirectory, 0);
    return this->AmlImg_unpack_private(ImagePath, 0, 0, 0, ImageDirectory, 0);
}

__s32 CAmlImagePack::AmlImg_unpack_addSecure(const char *ImagePath, const char *ImageDirectory, int addSecure) {
    return this->AmlImg_unpack_private(ImagePath, 0, 0, 0, ImageDirectory, addSecure);
}

__s32 CAmlImagePack::AmlImg_unpack_filetype(const char *ImagePath, const char *FileType, const char *ImageDirectory) {
    return this->AmlImg_unpack_private(ImagePath, 0, 0, FileType, ImageDirectory, 0);
}

__s32 CAmlImagePack::AmlImg_unpack_maintype(const char *ImagePath, const char *MainType, const char *ImageDirectory) {
    return this->AmlImg_unpack_private(ImagePath, MainType, 0, 0, ImageDirectory, 0);
}

__s32 CAmlImagePack::AmlImg_unpack_subtype(const char *ImagePath, const char *SubType, const char *ImageDirectory) {
    return this->AmlImg_unpack_private(ImagePath, 0, SubType, 0, ImageDirectory, 0);
}

__s32 CAmlImagePack::AmlImg_unpack_private(const char *ImagePath, const char *MainType, const char *SubType, const char *FileType, const char *ImageDirectory, int addSecure) {
    FILE *fp;
    HIMAGE hImage;
    ItemInfo *hItem;
    char *Buffer, *cfgline, *ptr;
    __u64 item_size, bytes_rw;
    __s32 backup_itemId, result;
    __u32 itemId, item_count, verifyId, is_verify_item, wlen, slen;
    char itemMainType[ITEM_NAME_LEN_V2], itemSubType[ITEM_NAME_LEN_V2], itemFileType[ITEM_NAME_LEN_V2];
    char FilePath[ITEM_NAME_LEN_V2], backup_SubType[ITEM_NAME_LEN_V2], backup_MainType[ITEM_NAME_LEN_V2];
    struct {char *data; char *ptr;} list_normal, list_verify;
    char *secure[3][3] = {
        //file          maintype    subtype
        {"usb.bl2", "USB",      "DDR_ENC"},
        {"usb.tpl", "USB",      "UBOOT_ENC"},
        {"sd.bin",  "UBOOT.ENC","aml_sdc_burn"},
    };

    if (!(hImage = this->AmlImg_open(ImagePath))) {
        MESSAGE_ERR("AmlImg_open return NULL!\n"); //L1631
        return -1;
    }

    if (this->AmlImg_check(hImage)) { //if (this->AmlImg_check(ImagePath)) {
        MESSAGE_ERR("Image file check failed!\n"); //L1637
        this->AmlImg_close(hImage);
        return -1;
    }

    if (this->version == AML_FRMWRM_VER_V1 || this->version == AML_FRMWRM_VER_V2) {
        cfgline = CONFIG_LINE;
    } else {
        this->AmlImg_close(hImage);
        return -1;
    }

    result = 0;
    memset(FilePath, 0, sizeof(FilePath));
    ptr = FilePath;
    while ((*ptr++ = *ImageDirectory++));
    *--ptr = '\\';
    *++ptr = 0;

    item_count = this->AmlImg_get_item_count(hImage, 0);
    verifyId = item_count - 2 * this->AmlImg_get_item_count(hImage, "VERIFY");
    if (item_count) {
        if (!(Buffer = (char*)malloc(RW_MAX_SIZE))) {
            MESSAGE_ERR("Allocate memory for unpack failed!\n"); //L1648
            this->AmlImg_close(hImage);
            return -1;
        }

        list_normal.ptr = list_normal.data = (char*)malloc(verifyId * 1024);
        list_normal.ptr += sprintf(list_normal.ptr, TAG_NORMALLIST);
        list_normal.ptr += sprintf(list_normal.ptr, "\r\n");

        list_verify.ptr = list_verify.data = (char*)malloc((item_count - verifyId) * 1024);
        list_verify.ptr += sprintf(list_verify.ptr, TAG_VERIFYLIST);
        list_verify.ptr += sprintf(list_verify.ptr, "\r\n");

        memset(itemMainType, 0, sizeof(itemMainType));
        memset(itemSubType, 0, sizeof(itemSubType));
        memset(itemFileType, 0, sizeof(itemFileType));

        for (itemId = 0; itemId < item_count && !this->AmlImg_get_next_item(hImage, itemId, itemMainType, itemSubType, itemFileType); itemId++) {
            if (!strcmp("VERIFY", itemMainType) && (MainType ? strcmp(MainType, itemMainType) : 1) && (SubType ? strcmp(SubType, itemSubType) : 1))
                continue;

            if (!strcmp("aml_sysrecovery", itemSubType)) {
                MESSAGE("Unpack item [%-12s, %16s] entire of this image but skip its body size:%lld bytes\n", itemMainType, itemSubType, this->AmlImg_get_size(hImage));
                list_normal.ptr += sprintf(list_normal.ptr, cfgline, itemSubType, itemMainType, itemMainType, itemSubType, itemFileType);
            } else {
                slen = strlen(itemSubType);
                if (slen + strlen(FilePath) >= 0x100) {
                    MESSAGE_ERR("Out file length is too long!\n"); //L1712
                    result = -1;
                    break;
                }
                strcpy(ptr, itemSubType);
                ptr[slen++] = '.';
                ptr[slen] = 0;

                if (strlen(itemMainType) + strlen(FilePath) >= 0x100) {
                    MESSAGE_ERR("Out file length is too long!\n"); //L1725
                    result = -1;
                    break;
                }
                strcpy(&ptr[slen], itemMainType);
                hItem = (ItemInfo*)this->AmlImg_open_item(hImage, itemMainType, itemSubType);
                if (((FileType ? !strcmp(FileType, itemFileType) : 1) && this->AmlImg_is_backup_item(hItem) != 1) || (addSecure && !strcmp("bootloader", itemSubType) && !strcmp("PARTITION", itemMainType))) {
                    if (!(fp = fopen(FilePath, "wb+"))) {
                        MESSAGE_ERR("Open file %s for write failed\n", FilePath); //L1776
                        result = -1;
                        break;
                    }
                    item_size = this->AmlImg_get_item_size(hItem);
                    MESSAGE("Unpack item [%-12s, %16s] to (%s) size:%lld bytes\n", itemMainType, itemSubType, FilePath, item_size);
                    if (item_size) {
                        bytes_rw = 0;
                        while (bytes_rw < item_size) {
                            wlen = this->AmlImg_read_item_data(hImage, hItem, Buffer, RW_MAX_SIZE);
                            if (wlen != fwrite(Buffer, 1, wlen, fp))
                                break;
                            bytes_rw += wlen;
                            fflush(fp); //FIX
                        }
                        if (bytes_rw != item_size) {
                            MESSAGE_ERR("Write item data not complete!\n"); //L1798
                            result = -1;
                            break;
                        }
                    }
                    fclose(fp);

                    if (addSecure && !strcmp(itemMainType, "PARTITION") && !strcmp("_aml_dtb", itemSubType))
                        strcpy(&ptr[slen+strlen(itemMainType)], ".encrypt");

                    if (this->version == AML_FRMWRM_VER_V2) {
                        is_verify_item = this->AmlImg_is_verify_item(hItem);
                        if (is_verify_item == 0) {
                            list_normal.ptr += sprintf(list_normal.ptr, cfgline, itemSubType, &ptr[slen], itemMainType, itemSubType, itemFileType);
                        } else if (is_verify_item == 1) {
                            list_verify.ptr += sprintf(list_verify.ptr, cfgline, itemSubType, &ptr[slen], itemMainType, itemSubType, itemFileType);
                        } else {
                            MESSAGE_ERR("Check is verify exception!\n"); //L1831
                            result = -1;
                            break;
                        }
                    } else if (this->version == AML_FRMWRM_VER_V1) {
                        if (itemId < verifyId) {
                            list_normal.ptr += sprintf(list_normal.ptr, cfgline, itemSubType, &ptr[slen], itemMainType, itemSubType, itemFileType);
                        } else {
                            list_verify.ptr += sprintf(list_verify.ptr, cfgline, itemSubType, &ptr[slen], itemMainType, itemSubType, itemFileType);
                        }
                    }
                } else {
                    memset(backup_MainType, 0, sizeof(backup_MainType));
                    memset(backup_SubType, 0, sizeof(backup_SubType));
                    if (this->AmlImg_get_backup_itemId(hItem, &backup_itemId)) {
                        MESSAGE_ERR("Ecep:fail in get backup item id\n"); //L1748
                        result = -1;
                        break;
                    }
                    if (this->AmlImg_get_next_item(hImage, backup_itemId, backup_MainType, backup_SubType, itemFileType)) {
                        MESSAGE_ERR("Fail in get next item\n"); //L1753
                        result = -1;
                        break;
                    }
                    MESSAGE("Backup item [%-12s, %16s] backItemId[%d][%s, %s]\n", itemMainType, itemSubType, backup_itemId, backup_MainType, backup_SubType);
                    if (this->AmlImg_is_verify_item(hItem) == 1) {
                        list_verify.ptr += sprintf(list_verify.ptr, cfgline, backup_SubType, backup_MainType, itemMainType, itemSubType, itemFileType);
                    } else {
                        list_normal.ptr += sprintf(list_normal.ptr, cfgline, backup_SubType, backup_MainType, itemMainType, itemSubType, itemFileType);
                    }
                }
            }
        }
        free(Buffer);
    }

    if (addSecure) {
        itemId = 0;
        do {
            list_normal.ptr += sprintf(list_normal.ptr, cfgline, "bootloader", "PARTITION.encrypt", secure[itemId][0], secure[itemId][1], secure[itemId][2], "normal");
        } while (++itemId < (sizeof(secure) / sizeof(secure[0])));
        hItem = (ItemInfo*)this->AmlImg_open_item(hImage, "PARTITION", "_aml_dtb");
        MESSAGE("_aml_dtb.PARTITION DO %s Existed\n", (!hItem ? "NOT" : ""));
        if (hItem) list_normal.ptr += sprintf(list_normal.ptr, cfgline, "_aml_dtb", "PARTITION.encrypt", "dtb", "meson1_ENC", "normal");
    }

    this->AmlImg_close(hImage);

    if (!result) {
        strcpy(ptr, "image.cfg");
        if (!(fp = fopen(FilePath, "wb+"))) {
            MESSAGE_ERR("Open file %s failed!\n", FilePath); //L1916
            return -1;
        }
        list_normal.ptr += sprintf(list_normal.ptr, "\r\n");
        slen = list_normal.ptr - list_normal.data;
        wlen = list_verify.ptr - list_verify.data;
        if (slen == fwrite(list_normal.data, 1, slen, fp) && wlen == fwrite(list_verify.data, 1, wlen, fp)) {
            MESSAGE("Write config file \"%s\" OK!\n", FilePath);
        } else {
            MESSAGE_ERR("Write config file buf failed!\n"); //L1922
            result = -1;
        }
        fclose(fp);
    }
    if (list_normal.data) free(list_normal.data);
    if (list_verify.data) free(list_verify.data);
    return result;
}