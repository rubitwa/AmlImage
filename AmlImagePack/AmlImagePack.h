#ifndef __CAMLIMAGEPACK_H__
#define __CAMLIMAGEPACK_H__

#include <stdio.h>
#include "amlImage_if.h"
//#include <vector>

#define AMLIMAGEPACK_DLL_VERSION_320	0x57C
#define AMLIMAGEPACK_DLL_VERSION_224	0x5A0

#pragma pack(push,4)
typedef struct FileList_s
{
    struct FileList_s *next;
    char key[ITEM_NAME_LEN_V2 * 2]; //std::string key = itemSubType + "." + itemMainType
    ItemInfo item;
    char name[256];
    __s32 genSha1Sum;
} FileList;
#pragma pack(pop)

#ifndef __FILE_NAME__
#define __FILE_NAME__ __FILE__
#endif

#define MESSAGE(...) fprintf(stdout, "[Msg]"),fprintf(stdout, __VA_ARGS__)
#define MESSAGE_EXT(...) fprintf(stdout, __VA_ARGS__)
#define MESSAGE_ERR(...) fprintf(stdout, "[ERR]f(%s)L%d:", __FILE_NAME__, __LINE__),fprintf(stdout, __VA_ARGS__)
//#define MESSAGE_ERR(...) sprintf_s(this->errmsg, sizeof(this->errmsg), "[ERR]f(%s)L%d:", __FILE_NAME__, __LINE__)

/* CAmlImagePack version 3.2.0
  char errmsg[256]; //256
  unsigned int crc_table[256]; //1024
  int version; //4
  AmlFirmwareImg_t *header; //4
  std::vector<ItemInfo_V1*> itemheads_v1; //12
  std::vector<ItemInfo_V2*> itemheads_v2; //12
  char mapItem[8]; //8 rbtree? + count
  char mapItem_verify[8]; //8 same as mapItem? VERIFY list?
  FILE *fp; //4
  std::string ConfigPath; //24
  std::string ImageDirectory; //24
  std::string ImagePath; //24
//1404 0x57C */
#pragma pack(push, 4)
class DLL_EXPORT CAmlImagePack {
private:
	char errmsg[256 - sizeof(ItemInfo *) - sizeof(FileList *) - sizeof(__u32) - sizeof(__u32) - sizeof(__u32)]; //v3.2.0 works
	ItemInfo *itemheads;
	FileList *mapItem;
	__u32 mapItemCount;
	__u32 mapItemVersion; // AmlImg_cfg_parse() not give image version for AmlImg_pack_create_mapItem()
public:
	__u32 mapItemSorting; // c++ containers using sorting
private:
	__u32 crc_table[256]; //v3.2.0 works
public:
	__u32 version; //v3.2.0 works
	AmlFirmwareImg_t *header; //v3.2.0 works
private:
	char _itemheads_v1[12];//std::vector<ItemInfo_V1*> itemheads_v1; //v3.2.0 works
	char _itemheads_v2[12];//std::vector<ItemInfo_V2*> itemheads_v2; //v3.2.0 works
	char _mapItem[8];
	char _mapItem_verifed[8];
	FILE *fp; //v3.2.0 works
	char std_string_ConfigPath[24];
	char std_string_ImageDirectory[24];
	char std_string_ImagePath[24];
	char reserved[0x5A0 - 0x57C]; // v2.2.4 compatible
	//v3.2.0 sizeof(CAmlImagePack) = 0x57C
	//v2.2.4 sizeof(CAmlImagePack) = 0x5A0
private:
	void init_crc_table(void);
	__u32 crc32(__u32 crc, unsigned char *buffer, __u32 size);
	__u32 calc_img_crc(FILE *fp, off_t offset);
	void AmlImg_clear(void);
	__s32 AmlImg_cfg_parse(const char *ConfigPath, __s32 version);
	__s32 AmlImg_pack_create_mapItem(const char *FileName, const char *MainType, const char *SubType, const char *FileType, int verify, int GenSha1Sum);
public:
	//CAmlImagePack(class CAmlImagePack const &);
	CAmlImagePack(void);
	~CAmlImagePack(void);
	class CAmlImagePack &operator =(class CAmlImagePack const &);
	const char *AmlImg_get_errmsg(void) { return this->errmsg; };

	HIMAGE AmlImg_open(const char *ImagePath);
	__s32 AmlImg_check(HIMAGE hImage);
	__s32 AmlImg_check(const char *ImagePath);
	__u64 AmlImg_get_size(HIMAGE hImage);

	__s32 AmlImg_pack(const char *ConfigPath, const char *ImageDirectory, const char *ImagePath, __s32 version);
	__s32 AmlImg_unpack(const char *ImagePath, const char *ImageDirectory);
	__s32 AmlImg_unpack_addSecure(const char *ImagePath, const char *ImageDirectory, int addSecure);
	__s32 AmlImg_unpack_filetype(const char *ImagePath, const char *FileType, const char *ImageDirectory);
	__s32 AmlImg_unpack_maintype(const char *ImagePath, const char *MainType, const char *ImageDirectory);
	__s32 AmlImg_unpack_subtype(const char *ImagePath, const char *SubType, const char *ImageDirectory);
	__s32 AmlImg_unpack_private(const char *ImagePath, const char *MainType, const char *SubType, const char *FileType, const char *ImageDirectory, int addSecure);

	__s32 AmlImg_close(HIMAGE hImage);
	HIMAGEITEM AmlImg_open_item(HIMAGE hImage, const char *MainType, const char *SubType);

	__s32 AmlImg_get_item_count(HIMAGE hImage, const char *MainType);
	__u64 AmlImg_get_item_size(HIMAGEITEM hItem);
	const char *AmlImg_get_item_type(HIMAGEITEM hItem);
	__u32 AmlImg_is_backup_item(HIMAGEITEM hItem);
	__u32 AmlImg_is_verify_item(HIMAGEITEM hItem);
	__s32 AmlImg_item_seek(HIMAGE hImage, HIMAGEITEM hItem, __s64 offset, __u32 origin);
	__u32 AmlImg_read_item_data(HIMAGE hImage, HIMAGEITEM hItem, void *buff, __u32 readSz);
	__s32 AmlImg_close_item(HIMAGEITEM hItem);
	__s32 AmlImg_get_backup_itemId(HIMAGEITEM hItem, __s32 *backUpItemId);
	__s32 AmlImg_get_next_item(HIMAGE hImage, __u32 ItemId, char *MainType, char *SubType, char *fileType);
};
#pragma pack(pop)
#endif // __CAMLIMAGEPACK_H__
