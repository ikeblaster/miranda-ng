#ifndef _CLOUDSERVICE_DROPBOX_H_
#define _CLOUDSERVICE_DROPBOX_H_

class CDropboxService : public CCloudService
{
private:
	static unsigned RequestAccessTokenThread(void *owner, void *param);
	static unsigned __stdcall RevokeAccessTokenThread(void *param);

	void HandleJsonError(JSONNode &node) override;

	void UploadFile(const char *data, size_t size, CMStringA &path);
	void CreateUploadSession(const char *chunk, size_t chunkSize, CMStringA &sessionId);
	void UploadFileChunk(const char *chunk, size_t chunkSize, const char *sessionId, size_t offset);
	void CommitUploadSession(const char *chunk, size_t chunkSize, const char *sessionId, size_t offset, CMStringA &path);
	void CreateFolder(const char *path);
	void CreateSharedLink(const char *path, CMStringA &url);

public:
	CDropboxService(const char *protoName, const wchar_t *userName);

	static CDropboxService* Init(const char *szModuleName, const wchar_t *szUserName);
	static int UnInit(CDropboxService*);

	const char* GetModuleName() const override;

	int GetIconId() const override;

	bool IsLoggedIn() override;
	void Login() override;
	void Logout() override;

	UINT Upload(FileTransferParam *ftp) override;
};

#endif //_CLOUDSERVICE_DROPBOX_H_