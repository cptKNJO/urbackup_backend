#pragma once

#include "IFSImageFactory.h"

class FSImageFactory : public IFSImageFactory
{
public:
	virtual IFilesystem *createFilesystem(const std::string &pDev, EReadaheadMode read_ahead,
		bool background_priority, std::string orig_letter, IFsNextBlockCallback* next_block_callback);

	virtual IVHDFile *createVHDFile(const std::string &fn, bool pRead_only, uint64 pDstsize,
		unsigned int pBlocksize=2*1024*1024, bool fast_mode=false, IFSImageFactory::ImageFormat format=IFSImageFactory::ImageFormat_VHD,
		size_t n_compress_threads=0);

	virtual IVHDFile *createVHDFile(const std::string &fn, const std::string &parent_fn,
		bool pRead_only, bool fast_mode=false, IFSImageFactory::ImageFormat format=IFSImageFactory::ImageFormat_VHD, uint64 pDstsize=0,
		size_t n_compress_threads = 0);

	virtual void destroyVHDFile(IVHDFile *vhd);

	virtual IReadOnlyBitmap* createClientBitmap(const std::string& fn);

	virtual IReadOnlyBitmap* createClientBitmap(IFile* bitmap_file);

	virtual bool initializeImageMounting();

	virtual std::vector<SPartition> readPartitions(IVHDFile *vhd, int64 offset, bool& gpt_style);

	virtual std::vector<SPartition> readPartitions(IFile* dev, bool& gpt_style);

	virtual std::vector<SPartition> readPartitions(const std::string& mbr,
		const std::string& gpt_header, const std::string& gpt_table, bool& gpt_style);

private:
	bool isNTFS(char *buffer);
};
