#include "stdio.h"
#include "stdlib.h"
#include <iostream>
#include <string.h>

#ifndef __FILE_NAME__
#define __FILE_NAME__ "AmlImage.cpp"
#endif
#include "AmlImagePack.h"

using namespace std;

void Usage(const char *ProgName) {
	printf("This program for Amlogic Firmware Files\n\n"
		"USAGE: %s -i C:\\Path\\To\\ImageFile.img [-u] [-d C:\\Path\\To\\Directory] [-s] [--item 3] [--check]\n"
		"  %s -i C:\\Path\\To\\ImageFile.img -c C:\\Path\\To\\Image.cfg -d C:\\Path\\To\\Directory [--sorting 0] [--version 2]\n"
		"  Where:\n"
		"    -i, --image <Image File Path>   Path to Image File\n\n"

		"    -s, --show                      flag for show information about Image File\n"
		"                                    required params --image.\n\n"

		"    --check                         flag for verify Image File\n"
		"                                    required params --image.\n\n"

		"    -u, --unpack                    flag for unpack Image File.\n"
		"                                    required params --image, --dir.\n\n"

		"    -c, --config <Config File Path> Path to Config File for create Image File\n"
		"                                    required params --image, --dir.\n\n"

		"    -d, --dir <Directory Path>      Path to directory where Image Files are stored.\n"
		"                                    If not set, using current directory '.\\'\n\n"

		"    -v, --version <Version>         Select version for create Image File, %d or %d (default)\n\n"

		"    --item <itemId>             Select one item in Image File for unpack or show information\n"
		"                                required params --image.\n\n"

		"    --sorting                   Sorting files when create Image File (working only with reversed AmlImagePack.dll)\n"
		"                                -1 by default, if --version 1 soring is 0, --version != 1 sorting is 1\n\n"

		"    --addSecure                 flag for unpack.\n\n", ProgName, ProgName, AML_FRMWRM_VER_V1, AML_FRMWRM_VER_V2);
}

#define CONFIG_LINE "file=\"%s.%s\"\t\tmain_type=\"%s\"\t\tsub_type=\"%s\"\tfile_type=\"%s\"\r\n"
int main(int argc, char *argv[])
{
	int result = 0;
	HIMAGE hImage = 0;
	CAmlImagePack *aml = 0;
	ItemInfo *hItem, *hItemVerify;
	FILE *fp;
	size_t slen, wlen;
	__u64 itemSz, bytes_rw;
	__u32 is_verify_item, version = AML_FRMWRM_VER_V2;
	__s32 backup_itemId, mapItemSorting = -1, loop, itemCount, verifyId, itemId = -1;
	char *ProgName, *ConfigPath = 0, *ImageDirectory = 0, *ImagePath = 0, *Buffer = 0;
	char UnpackImage = 0, VerifyImage = 0, ShowInformation = 0, addSecure = 0;
	char itemMainType[ITEM_NAME_LEN_V2], itemSubType[ITEM_NAME_LEN_V2], itemFileType[ITEM_NAME_LEN_V2], FilePath[ITEM_NAME_LEN_V2];
	char backup_MainType[ITEM_NAME_LEN_V2], backup_SubType[ITEM_NAME_LEN_V2], itemSha1Sum[ITEM_VERIFY_LEN + 1], *cfgline, *ptr = 0;
	struct { char *data; char *ptr; } list_normal = { 0, 0 }, list_verify = { 0, 0 };
	char *secure[3][3] = {
		{ "usb.bl2", "USB",      "DDR_ENC" },
		{ "usb.tpl", "USB",      "UBOOT_ENC" },
		{ "sd.bin",  "UBOOT.ENC","aml_sdc_burn" },
	};

	ProgName = *argv;
	while (*ProgName != 0) ProgName++;
	while (*ProgName != '\\' && ProgName != *argv) ProgName--;
	if (*ProgName == '\\') ProgName++;

	if (argc == 1) {
		Usage(ProgName);
		return 0;
	}

	while (--argc > 0 && *++argv) {
		if (!strcmp(*argv, "--image") || !strcmp(*argv, "-i")) {
			ImagePath = *++argv;
		} else if (!strcmp(*argv, "--item")) {
			itemId = atoi(*++argv);
		} else if (!strcmp(*argv, "--unpack") || !strcmp(*argv, "-u")) {
			UnpackImage = 1;
			if (!(Buffer = (char*)malloc(RW_MAX_SIZE))) {
				MESSAGE_ERR("Allocate memory for unpack failed!\n");
				return -1;
			}
			ImageDirectory = *++argv;
		} else if (!strcmp(*argv, "--config") || !strcmp(*argv, "-c")) {
			ConfigPath = *++argv;
		} else if (!strcmp(*argv, "--dir") || !strcmp(*argv, "-d")) {
			ImageDirectory = *++argv;
		} else if (!strcmp(*argv, "--version") || !strcmp(*argv, "-v")) {
			version = atoi(*++argv);
		} else if (!strcmp(*argv, "--check")) {
			VerifyImage = 1;
		} else if (!strcmp(*argv, "--show") || !strcmp(*argv, "-s")) {
			ShowInformation = 1;
		} else if (!strcmp(*argv, "--sorting")) {
			mapItemSorting = atoi(*++argv);
		} else if (!strcmp(*argv, "--addSecure")) {
			addSecure = 1;
		} else if (!strcmp(*argv, "--help") || !strcmp(*argv, "-h")) {
			Usage(ProgName);
		}
	}

	cfgline = CONFIG_LINE;
	memset(itemMainType, 0, sizeof(itemMainType));
	memset(itemSubType, 0, sizeof(itemSubType));
	memset(itemFileType, 0, sizeof(itemFileType));
	memset(itemSha1Sum, 0, sizeof(itemSha1Sum));

	aml = new CAmlImagePack();
	if (mapItemSorting != -1)
		aml->mapItemSorting = mapItemSorting;

	if (ImageDirectory == 0)
		ImageDirectory = ".\\";

	if (ImagePath && !ConfigPath) {
		if (ShowInformation) {
			MESSAGE("Image: %s\n", ImagePath);
		}
		if (!(hImage = aml->AmlImg_open(ImagePath)))
			result = -1;
	} else if (ConfigPath && ImageDirectory && ImagePath) {
		return aml->AmlImg_pack(ConfigPath, ImageDirectory, ImagePath, version);
	}

	if (hImage) {
		if (ShowInformation) {
			MESSAGE("CRC: %u (0x%lx)\n", aml->header->crc, aml->header->crc);
			MESSAGE("Magic: %u (0x%lx)\n", aml->header->magic, aml->header->magic);
			//MESSAGE("Version: %u\n", aml->header->version); // version auto printed by CAmlImagePack::AmlImg_open()
			MESSAGE("Size: %lluMb (0x%llx)\n", (aml->header->imageSz >> 20), aml->header->imageSz);
			//MESSAGE("ItemAlginSize: %u\n", aml->header->itemAlginSize);
			MESSAGE("Items: %u\n", aml->AmlImg_get_item_count(hImage, 0));
		}

		if (ImageDirectory) {
			memset(FilePath, 0, sizeof(FilePath));
			ptr = FilePath;
			while ((*ptr++ = *ImageDirectory++));
			*--ptr = '\\';
			*++ptr = 0;
		}

		if (VerifyImage) {
			if ((result = aml->AmlImg_check(hImage))) {
				MESSAGE("Image file check failed!\n");
			} else {
				MESSAGE("Image file check success.\n");
			}
		}

		itemCount = aml->AmlImg_get_item_count(hImage, 0);
		verifyId = itemCount - 2 * aml->AmlImg_get_item_count(hImage, "VERIFY");
		if (UnpackImage && ImageDirectory && itemId == -1) {
			list_normal.ptr = list_normal.data = (char*)malloc(verifyId * 1024);
			if (!list_normal.data) {
				MESSAGE_ERR("Allocate memory for image config failed!\n");
				aml->AmlImg_close(hImage);
				return -1;
			}
			list_normal.ptr += sprintf(list_normal.ptr, "[LIST_NORMAL]");
			list_normal.ptr += sprintf(list_normal.ptr, "\r\n");

			list_verify.ptr = list_verify.data = (char*)malloc((itemCount - verifyId) * 1024);
			if (!list_verify.data) {
				MESSAGE_ERR("Allocate memory for image config failed!\n");
				aml->AmlImg_close(hImage);
				return -1;
			}
			list_verify.ptr += sprintf(list_verify.ptr, "[LIST_VERIFY]");
			list_verify.ptr += sprintf(list_verify.ptr, "\r\n");
		}

		if (itemId == -1) {
			itemId = 0;
			loop = itemCount;
		} else {
			loop = 1;
		}

		while (!result && loop-- && !aml->AmlImg_get_next_item(hImage, itemId, itemMainType, itemSubType, itemFileType)) {
			if (!(hItem = (ItemInfo*)aml->AmlImg_open_item(hImage, itemMainType, itemSubType)))
				break;

			is_verify_item = 0;
			if (aml->version == AML_FRMWRM_VER_V1) {
				is_verify_item = (itemId >= verifyId ? 1 : 0);
			} else if (aml->version == AML_FRMWRM_VER_V2) {
				is_verify_item = aml->AmlImg_is_verify_item(hItem);
				if (is_verify_item != 0 && is_verify_item != 1) {
					MESSAGE_ERR("Check is verify exception!\n");
					result = -1;
					break;
				}
			}

			if (ShowInformation) {
				MESSAGE("----------------------------------------\n");
				MESSAGE("ItemId: %u\n", hItem->itemId);
				if (aml->version == AML_FRMWRM_VER_V1) {
					MESSAGE("MainType: %s\n", hItem->v1.itemMainType);
					MESSAGE("SubType: %s\n", hItem->v1.itemSubType);
				} else if (aml->version == AML_FRMWRM_VER_V2) {
					MESSAGE("MainType: %s\n", hItem->v2.itemMainType);
					MESSAGE("SubType: %s\n", hItem->v2.itemSubType);
				}
				MESSAGE("FileType: %s\n", itemFileType);
				MESSAGE("Offset: %llu (0x%llx)\n", hItem->offsetInImage, hItem->offsetInImage);
				itemSz = hItem->itemSz >> 20;
				MESSAGE("Size: %llu%s (0x%llx)\n", (itemSz ? itemSz : hItem->itemSz), (itemSz ? "Mb" : "b"), hItem->itemSz);
				if (aml->version == AML_FRMWRM_VER_V1) {
					MESSAGE("Is Backup: %s\n", (hItem->v1.isBackUpItem ? "true" : "false"));
					if (hItem->v1.isBackUpItem)
						MESSAGE("Backup ItemId: %u\n", hItem->v1.backUpItemId);
				} else if (aml->version == AML_FRMWRM_VER_V2) {
					MESSAGE("Is Backup: %s\n", (hItem->v2.isBackUpItem ? "true" : "false"));
					if (hItem->v2.isBackUpItem)
						MESSAGE("Backup ItemId: %u\n", hItem->v2.backUpItemId);
				}
			}

			if (is_verify_item && strcmp(itemMainType, "VERIFY")) {
				if ((hItemVerify = (ItemInfo*)aml->AmlImg_open_item(hImage, "VERIFY", itemSubType))) {
					if (ShowInformation) {
						if (hItemVerify->itemSz == ITEM_VERIFY_LEN && aml->AmlImg_read_item_data(hImage, hItemVerify, itemSha1Sum, (__u32)hItemVerify->itemSz) == hItemVerify->itemSz) {
							MESSAGE("Verify: %s\n", itemSha1Sum);
						} else {
							MESSAGE("Verify: err size != %u\n", ITEM_VERIFY_LEN);
						}
					} else {
						hItemVerify->curoffsetInItem = hItemVerify->itemSz;
					}
					aml->AmlImg_close_item(hItemVerify);
				}
			}

			if (UnpackImage && ImageDirectory && hItem->itemSz != hItem->curoffsetInItem) {
				if (aml->AmlImg_is_backup_item(hItem) != 1 || (addSecure && !strcmp("bootloader", itemSubType) && !strcmp("PARTITION", itemMainType))) {
					slen = strlen(itemSubType);
					strcpy(ptr, itemSubType);
					ptr[slen++] = '.';
					ptr[slen] = 0;
					if (strlen(itemMainType) + strlen(FilePath) >= sizeof(FilePath)) {
						MESSAGE_ERR("Out file length is too long!\n");
						break;
					}
					strcpy(&ptr[slen], itemMainType);
					if ((fp = fopen(FilePath, "wb+"))) {
						itemSz = aml->AmlImg_get_item_size(hItem);
						MESSAGE("Unpack item %i [%-12s, %16s] to (%s) size:%lld bytes\n", hItem->itemId, itemMainType, itemSubType, FilePath, itemSz);
						if (itemSz) {
							bytes_rw = 0;
							while (bytes_rw < itemSz) {
								wlen = aml->AmlImg_read_item_data(hImage, hItem, Buffer, RW_MAX_SIZE);
								if (wlen != fwrite(Buffer, 1, wlen, fp))
									break;
								bytes_rw += wlen;
								fflush(fp);
							}
							if (bytes_rw != itemSz) {
								MESSAGE_ERR("Write item data not complete!\n"); //L1798
								break;
							}
						}
						fclose(fp);

						if (addSecure && !strcmp(itemMainType, "PARTITION") && !strcmp("_aml_dtb", itemSubType))
							strcpy(&ptr[slen + strlen(itemMainType)], ".encrypt");

						if (list_normal.data && strcmp(itemMainType, "VERIFY")) {
							if (is_verify_item) {
								list_verify.ptr += sprintf(list_verify.ptr, cfgline, itemSubType, itemMainType, itemMainType, itemSubType, itemFileType);
							} else {
								list_normal.ptr += sprintf(list_normal.ptr, cfgline, itemSubType, itemMainType, itemMainType, itemSubType, itemFileType);
							}
						}
					} else {
						MESSAGE_ERR("Open file %s for write failed\n", FilePath);
						break;
					}
				} else if (list_normal.data && list_verify.data) {
					memset(backup_MainType, 0, sizeof(backup_MainType));
					memset(backup_SubType, 0, sizeof(backup_SubType));
					if (aml->AmlImg_get_backup_itemId(hItem, &backup_itemId)) {
						MESSAGE_ERR("Ecep:fail in get backup item id\n");
						result = -1;
						break;
					}
					if (aml->AmlImg_get_next_item(hImage, backup_itemId, backup_MainType, backup_SubType, itemFileType)) {
						MESSAGE_ERR("Fail in get next item\n");
						result = -1;
						break;
					}
					MESSAGE("Backup item [%-12s, %16s] backItemId[%d][%s, %s]\n", itemMainType, itemSubType, backup_itemId, backup_MainType, backup_SubType);
					if (aml->AmlImg_is_verify_item(hItem) == 1) {
						list_verify.ptr += sprintf(list_verify.ptr, cfgline, backup_SubType, backup_MainType, itemMainType, itemSubType, itemFileType);
					} else {
						list_normal.ptr += sprintf(list_normal.ptr, cfgline, backup_SubType, backup_MainType, itemMainType, itemSubType, itemFileType);
					}
				}
			}
			aml->AmlImg_close_item(hItem);
			itemId++;
		}

		if (addSecure) {
			itemId = 0;
			do {
				list_normal.ptr += sprintf(list_normal.ptr, cfgline, "bootloader.PARTITION.encrypt", secure[itemId][0], secure[itemId][1], secure[itemId][2], "normal");
			} while (++itemId < (sizeof(secure) / sizeof(secure[0])));
			hItem = (ItemInfo*)aml->AmlImg_open_item(hImage, "PARTITION", "_aml_dtb");
			MESSAGE("_aml_dtb.PARTITION DO %s Existed\n", (!hItem ? "NOT" : ""));
			if (hItem) list_normal.ptr += sprintf(list_normal.ptr, cfgline, "_aml_dtb", "PARTITION.encrypt", "dtb", "meson1_ENC", "normal");
		}

		aml->AmlImg_close(hImage);

		if (!result && UnpackImage && ImageDirectory && list_normal.data && list_verify.data) {
			strcpy(ptr, "image.cfg");
			if ((fp = fopen(FilePath, "wb+"))) {
				list_normal.ptr += sprintf(list_normal.ptr, "\r\n");
				slen = list_normal.ptr - list_normal.data;
				wlen = list_verify.ptr - list_verify.data;
				if (slen == fwrite(list_normal.data, 1, slen, fp) && wlen == fwrite(list_verify.data, 1, wlen, fp)) {
					MESSAGE("Write config file \"%s\" OK!\n", FilePath);
				} else {
					MESSAGE_ERR("Write config file buf failed!\n");
					result = -1;
				}
				fclose(fp);
			} else {
				MESSAGE_ERR("Open file %s failed!\n", FilePath);
				result = -1;
			}
		}
		if (list_normal.data)
			free(list_normal.data);
		if (list_verify.data)
			free(list_verify.data);
	}

	if (Buffer)
		free(Buffer);

	return result;
}


