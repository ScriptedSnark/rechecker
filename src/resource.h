#pragma once

#define FILE_INI_RESOURCES	"resources.ini"
#define MAX_CMD_LENGTH		128

enum flag_type_resources
{
	FLAG_TYPE_NONE = 0,
	FLAG_TYPE_EXISTS,		// to comparison with the specified hash value
	FLAG_TYPE_MISSING,		// check it missing file on client
	FLAG_TYPE_IGNORE,		// ignore the specified hash value
	FLAG_TYPE_HASH_ANY,		// any file with any the hash value
};

enum arg_type_e
{
	ARG_TYPE_FILE_NAME = 0,
	ARG_TYPE_FILE_HASH,
	ARG_TYPE_CMD_EXEC,
	ARG_TYPE_FLAG,

	MAX_PARSE_ARGUMENT,
};

// buffer for checker list
class CResourceBuffer
{
public:
	CResourceBuffer(char *filename, char *cmdExec, flag_type_resources flag, uint32 hash, int line, bool bBreak);

	uint32 GetFileHash() const { return m_FileHash; };
	flag_type_resources GetFileFlag() const { return m_Flag; };

	const char *GetFileName() const { return m_FileName; };
	const char *GetCmdExec() const { return m_CmdExec; };
	int GetLine() const { return m_Line; };

	bool IsBreak() const { return m_Break; };
	bool IsDuplicate() const { return m_Duplicate; };
	void SetDuplicate() { m_Duplicate = true; };

private:
	uint32 m_FileHash;

	flag_type_resources m_Flag;
	int m_Line;

	const char *m_FileName;
	const char *m_CmdExec;

	bool m_Duplicate;	// for to check for duplicate
	bool m_Break;		// do not check a next files
};

class CResourceFile
{
public:
	void Init();
	void Clear(IGameClient *pClient = NULL);
	void LoadResources();
	void CreateResourceList();
	void Log(const char *fmt, ...);

	bool FileConsistencyResponse(IGameClient *pSenderClient, resource_t *resource, uint32 hash);

private:
	// buffer for response list
	class ResponseBuffer
	{
	public:
		ResponseBuffer(IGameClient *pSenderClient, char *filename, uint32 hash);

		IGameClient *GetGameClient() const { return m_pClient; };
		const char *GetFileName() const { return m_FileName; };
		uint32 GetClientHash() const { return m_ClientHash; };

	private:
		IGameClient *m_pClient;
		const char *m_FileName;
		uint32 m_ClientHash;
	};

private:
	// for temporary files of responses
	void AddElement(char *filename, char *cmdExec, flag_type_resources flag, uint32 hash, int line, bool bBreak);
	void AddFileResponse(IGameClient *pSenderClient, char *filename, uint32 hash);
	const char *FindFilenameOfHash(uint32 hash);
	void LogPrepare();

	// parse
	const char *GetNextToken(char **pbuf);

private:
	typedef std::vector<CResourceBuffer *> ResourceList;
	typedef std::vector<ResponseBuffer *> ResponseList;

	ResourceList m_resourceList;
	ResponseList m_responseList;

	int m_DecalsNum;
	uint32 m_PrevHash;

	char m_PathDir[MAX_PATH_LENGTH];
	char m_LogFilePath[MAX_PATH_LENGTH];	// log data
};

extern CResourceFile Resource;

void ClearStringsCache();
