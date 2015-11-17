#include "precompiled.h"

CResourceFile Resource;
std::vector<const char *> StringsCache;

cvar_t cv_rch_log = { "rch_log", "0", 0, 0.0f, NULL };
cvar_t *pcv_rch_log = NULL;

void CResourceFile::CreateResourceList()
{
	int nConsistency = g_RehldsServerData->GetConsistencyNum();
	m_DecalsNum = g_RehldsServerData->GetDecalNameNum();

	for (auto iter = m_resourceList.cbegin(), end = m_resourceList.cend(); iter != end; ++iter)
	{
		CResourceBuffer *pRes = (*iter);

		// prevent duplicate of filenames
		// check if filename is been marked so do not add the resource again
		if (!pRes->IsDuplicate())
		{
			// check limit resource
			if (g_RehldsServerData->GetResourcesNum() >= MAX_RESOURCE_LIST)
			{
				UTIL_Printf(__FUNCTION__ ": can't add resource \"%s\" on line %d; exceeded the limit of resources max '%d'\n", pRes->GetFileName(), pRes->GetLine(), MAX_RESOURCE_LIST);
				break;
			}

			Log(__FUNCTION__ "  -> file: (%s), cmdexc: (%s), hash: (%x)", pRes->GetFileName(), pRes->GetCmdExec(), pRes->GetFileHash());
			SV_AddResource(t_decal, pRes->GetFileName(), 0, RES_CHECKFILE, m_DecalsNum++);
			nConsistency++;
		}
	}

	m_DecalsNum = g_RehldsServerData->GetDecalNameNum();
	g_RehldsServerData->SetConsistencyNum(nConsistency);
}

void CResourceFile::Clear(IGameClient *pClient)
{
	if (pClient != NULL)
	{
		auto iter = m_responseList.begin();
		while (iter != m_responseList.end())
		{
			ResponseBuffer *pFiles = (*iter);

			if (pFiles->GetGameClient() != pClient)
			{
				iter++;
				continue;
			}

			// erase cmdexec
			delete pFiles;
			iter = m_responseList.erase(iter);
		}

		m_PrevHash = 0;
		return;
	}

	m_PrevHash = 0;
	m_DecalsNum = 0;

	// clear resources
	m_resourceList.clear();

	ClearStringsCache();
}

void CResourceFile::Log(const char *fmt, ...)
{
	static char string[2048];

	FILE *fp;
	time_t td;
	tm *lt;
	char *file;
	char dateLog[64];
	bool bFirst = false;

	if (pcv_rch_log->string[0] != '1')
		return;

	fp = fopen(m_LogFilePath, "r");

	if (fp != NULL)
	{
		bFirst = true;
		fclose(fp);
	}

	fp = fopen(m_LogFilePath, "a");

	if (fp == NULL)
	{
		return;
	}

	va_list argptr;
	va_start(argptr, fmt);
	vsnprintf(string, sizeof(string), fmt, argptr);
	va_end(argptr);

	strcat(string, "\n");

	td = time(NULL);
	lt = localtime(&td);

	strftime(dateLog, sizeof(dateLog), "%m/%d/%Y - %H:%M:%S", lt);

	if (!bFirst)
	{
		file = strrchr(m_LogFilePath, '/');
		if (file == NULL)
			file = "<null>";

		fprintf(fp, "L %s: Log file started (file \"%s\") (version \"%s\")\n", dateLog, file, Plugin_info.version);
	}

	fprintf(fp, "L %s: %s", dateLog, string);
	fclose(fp);
}

void CreateDirectory(const char *path)
{
	_mkdir(path
#ifndef _WIN32
	,0755
#endif // _WIN32
	);
}

void CResourceFile::Init()
{
	char *pos;
	char path[MAX_PATH_LENGTH];

	strncpy(path, GET_PLUGIN_PATH(PLID), sizeof(path) - 1);
	path[sizeof(path) - 1] = '\0';

	pos = strrchr(path, '/');

	if (*pos == '\0')
		return;

	*(pos + 1) = '\0';

	strncpy(m_LogFilePath, path, sizeof(m_LogFilePath) - 1);
	m_LogFilePath[sizeof(m_LogFilePath) - 1] = '\0';
	strcat(m_LogFilePath, "logs/");
	CreateDirectory(m_LogFilePath);

	// resources.ini
	snprintf(m_PathDir, sizeof(m_PathDir), "%s" FILE_INI_RESOURCES, path);

	g_engfuncs.pfnCvar_RegisterVariable(&cv_rch_log);
	pcv_rch_log = g_engfuncs.pfnCVarGetPointer(cv_rch_log.name);
}

uint32 __declspec(naked) swap_endian(uint32 value)
{
	__asm
	{
		mov eax, dword ptr[esp + 4]
		bswap eax
		ret
	}
}

inline uint8 hexbyte(uint8 *hex)
{
	return ((hex[0] > '9' ? toupper(hex[0]) - 'A' + 10 : hex[0] - '0') << 4)
		| (hex[1] > '9' ? toupper(hex[1]) - 'A' + 10 : hex[1] - '0');
}

inline bool invalidchar(const char c)
{
	// to check for invalid characters
	return (c == '\\' || c == '/' || c == ':'
		|| c == '*' || c == '?'
		|| c == '"' || c == '<'
		|| c == '>' || c == '|') != 0;
}

bool IsValidFilename(char *psrc, char &pchar)
{
	char *pch = strrchr(psrc, '/');

	if (pch == NULL)
		pch = psrc;

	while (*pch++)
	{
		if (invalidchar(*pch))
		{
			pchar = *pch;
			return false;
		}
	}

	return true;
}

bool IsFileHasExtension(char *psrc)
{
	// find the extension filename
	char *pch = strrchr(psrc, '.');

	if (pch == NULL)
		return false;

	// the size extension
	if (strlen(&pch[1]) <= 0)
		return false;

	return strchr(pch, '/') == NULL;
}

void CResourceFile::LogPrepare()
{
	char dateFile[64];
	char *pos;
	time_t td;
	tm *lt;

	td = time(NULL);
	lt = localtime(&td);

	// remove path to log file
	if ((pos = strrchr(m_LogFilePath, '/')) != NULL)
	{
		*(pos + 1) = '\0';
	}

	strftime(dateFile, sizeof(dateFile), "L_%d_%m_%Y.log", lt);
	strcat(m_LogFilePath, dateFile);
}

void CResourceFile::LoadResources()
{
	char *pos;
	char line[4096];
	uint8 hash[16];
	FILE *fp;
	int argc;
	int len;
	flag_type_resources flag;
	char filename[MAX_PATH_LENGTH];
	char cmdBufExec[MAX_PATH_LENGTH];
	int cline = 0;
	bool bBreak;

	fp = fopen(m_PathDir, "r");

	if (fp == NULL)
	{
		UTIL_Printf(__FUNCTION__ ": can't find path to " FILE_INI_RESOURCES "\n");
		return;
	}

	while (!feof(fp) && fgets(line, sizeof(line), fp))
	{
		pos = line;

		cline++;

		if (*pos == '\0' || *pos == ';' || *pos == '\\' || *pos == '/' || *pos == '#')
			continue;

		const char *pToken = GetNextToken(&pos);

		argc = 0;
		bBreak = false;
		flag = FLAG_TYPE_NONE;

		memset(hash, 0, sizeof(hash));

		while (pToken != NULL && argc <= MAX_PARSE_ARGUMENT)
		{
			len = strlen(pToken);

			switch (argc)
			{
			case ARG_TYPE_FILE_NAME:
			{
				strncpy(filename, pToken, len);
				filename[len] = '\0';
				break;
			}
			case ARG_TYPE_FILE_HASH:
			{
				uint8 pbuf[33];

				strncpy((char *)pbuf, pToken, len);
				pbuf[len] = '\0';

				if (_stricmp((const char *)pbuf, "UNKNOWN") == 0)
				{
					flag = FLAG_TYPE_HASH_ANY;
				}
				else
				{
					for (int i = 0; i < sizeof(pbuf) / 2; i++)
						hash[i] = hexbyte(&pbuf[i * 2]);

					flag = (*(uint32 *)&hash[0] != 0x00000000) ? FLAG_TYPE_EXISTS : FLAG_TYPE_MISSING;
				}
				break;
			}
			case ARG_TYPE_CMD_EXEC:
			{
				strncpy(cmdBufExec, pToken, len);
				cmdBufExec[len] = '\0';

				if (_stricmp(cmdBufExec, "IGNORE") == 0)
				{
					flag = FLAG_TYPE_IGNORE;
					cmdBufExec[0] = '\0';
				}
				else if (_stricmp(cmdBufExec, "BREAK") == 0)
				{
					bBreak = true;
					cmdBufExec[0] = '\0';
				}
				else
				{
					// replface \' to "
					StringReplace(cmdBufExec, "'", "\"");
				}
				break;
			}
			case ARG_TYPE_FLAG:
			{
				if (_stricmp(pToken, "IGNORE") == 0)
				{
					flag = FLAG_TYPE_IGNORE;
				}
				else if (_stricmp(pToken, "BREAK") == 0)
				{
					bBreak = true;
				}
				break;
			}
			default:
				break;
			}

			argc++;
			pToken = GetNextToken(&pos);

			if (pToken == NULL && argc == ARG_TYPE_FLAG)
			{
				// go to next argument
				argc++;
			}
		}

		if (argc >= MAX_PARSE_ARGUMENT)
		{
			char pchar;
			if (strlen(filename) <= 0)
			{
				UTIL_Printf(__FUNCTION__ ": Failed to load \"" FILE_INI_RESOURCES "\"; path to filename is empty on line %d\n", cline);
				continue;
			}
			else if (!IsFileHasExtension(filename))
			{
				UTIL_Printf(__FUNCTION__ ": Failed to load \"" FILE_INI_RESOURCES "\"; filename has no extension on line %d\n", cline);
				continue;
			}
			else if (!IsValidFilename(filename, pchar))
			{
				UTIL_Printf(__FUNCTION__ ": Failed to load \"" FILE_INI_RESOURCES "\"; filename has invalid character '%c' on line %d\n", pchar, cline);
				continue;
			}
			else if (flag == FLAG_TYPE_NONE)
			{
				UTIL_Printf(__FUNCTION__ ": Failed to load \"" FILE_INI_RESOURCES "\"; parsing hash failed on line %d\n", cline);
				continue;
			}
			else if (strlen(cmdBufExec) <= 0 && (flag != FLAG_TYPE_IGNORE && !bBreak))
			{
				UTIL_Printf(__FUNCTION__ ": Failed to load \"" FILE_INI_RESOURCES "\"; parsing command line is empty on line %d\n", cline);
				continue;
			}

			AddElement(filename, cmdBufExec, flag, *(uint32 *)&hash[0], cline, bBreak);
		}
		else if (pToken != NULL || argc > ARG_TYPE_FILE_NAME)
		{
			UTIL_Printf(__FUNCTION__ ": Failed to load \"" FILE_INI_RESOURCES "\"; parsing not enough arguments on line %d (got '%d', expected '%d')\n", cline, argc, MAX_PARSE_ARGUMENT);
		}
	}

	fclose(fp);
	LogPrepare();
}

const char *CResourceFile::GetNextToken(char **pbuf)
{
	char *rpos = *pbuf;
	if (*rpos == '\0')
		return NULL;

	// skip spaces at the beginning
	while (*rpos != 0 && isspace(*rpos))
		rpos++;

	if (*rpos == '\0')
	{
		*pbuf = rpos;
		return NULL;
	}

	const char *res = rpos;
	char *wpos = rpos;
	char inQuote = '\0';

	while (*rpos != '\0')
	{
		char cc = *rpos;
		if (inQuote)
		{
			if (inQuote == cc)
			{
				inQuote = '\0';
				rpos++;
			}
			else
			{
				if (rpos != wpos)
					*wpos = cc;
				rpos++;
				wpos++;
			}
		}
		else if (isspace(cc))
		{
			break;
		}
		else if (cc == '\'' || cc == '"')
		{
			inQuote = cc;
			rpos++;
		}
		else
		{
			if (rpos != wpos)
				*wpos = cc;

			rpos++; wpos++;
		}
	}

	if (*rpos != '\0')
		rpos++;

	*pbuf = rpos;
	*wpos = '\0';
	return res;
}

void CResourceFile::AddElement(char *filename, char *cmdExec, flag_type_resources flag, uint32 hash, int line, bool bBreak)
{
	auto nRes = new CResourceBuffer(filename, cmdExec, flag, hash, line, bBreak);

	// to mark files which are not required to add to the resource again
	for (auto iter = m_resourceList.cbegin(), end = m_resourceList.cend(); iter != end; ++iter)
	{
		CResourceBuffer *pRes = (*iter);
		
		if (_stricmp(pRes->GetFileName(), filename) == 0)
		{
			// resource name already registered
			nRes->SetDuplicate();
			break;
		}
	}

	m_resourceList.push_back(nRes);
}

void CResourceFile::AddFileResponse(IGameClient *pSenderClient, char *filename, uint32 hash)
{
	m_responseList.push_back(new ResponseBuffer(pSenderClient, filename, hash));
}

bool CResourceFile::FileConsistencyResponse(IGameClient *pSenderClient, resource_t *resource, uint32 hash)
{
	bool bHandled = false;
	flag_type_resources typeFind;
	std::vector<CResourceBuffer *> tempResourceList;
	const char *hashFoundFile;
	const char *prevHashFoundFile;

	if (resource->type != t_decal
		|| resource->nIndex < m_DecalsNum)	// if by some miracle the decals will have the flag RES_CHECKFILE
							// to be sure not bypass the decals
	{
		AddFileResponse(pSenderClient, resource->szFileName, hash);
		m_PrevHash = hash;
		return true;
	}

	// strange thing
	// if this happened when missing all the files from client
	if (!m_PrevHash)
	{
		return true;
	}

	for (auto iter = m_resourceList.cbegin(), end = m_resourceList.cend(); iter != end; ++iter)
	{
		CResourceBuffer *pRes = (*iter);

		if (strcmp(resource->szFileName, pRes->GetFileName()) != 0)
			continue;

		typeFind = pRes->GetFileFlag();

		if (m_PrevHash == hash && typeFind != FLAG_TYPE_MISSING)
			typeFind = FLAG_TYPE_NONE;

		switch (typeFind)
		{
		case FLAG_TYPE_IGNORE:
			tempResourceList.push_back(pRes);
			break;
		case FLAG_TYPE_EXISTS:
			if (pRes->GetFileHash() != hash)
			{
				typeFind = FLAG_TYPE_NONE;
			}
			break;
		case FLAG_TYPE_HASH_ANY:
			for (auto it = tempResourceList.cbegin(); it != tempResourceList.cend(); ++it)
			{
				CResourceBuffer *pTemp = (*it);

				if (_stricmp(pTemp->GetFileName(), pRes->GetFileName()) != 0)
					continue;

				if (pTemp->GetFileHash() == hash)
				{
					typeFind = FLAG_TYPE_NONE;
					break;
				}
			}
			break;
		case FLAG_TYPE_MISSING:
			if (m_PrevHash != hash)
			{
				typeFind = FLAG_TYPE_NONE;
			}
			break;
		default:
			typeFind = FLAG_TYPE_NONE;
			break;
		}

		if (typeFind != FLAG_TYPE_NONE)
		{
			if (hash != 0x0)
			{
				// push exec cmd
				Exec.AddElement(pSenderClient, pRes, hash);
			}

			hashFoundFile = FindFilenameOfHash(hash);
			prevHashFoundFile = FindFilenameOfHash(m_PrevHash);

			if (prevHashFoundFile == NULL)
				prevHashFoundFile = "null";

			if (hashFoundFile == NULL)
				hashFoundFile = "null";

			Log("  -> file: (%s), exphash: (%x), got: (%x), typeFind: (%d), prevhash: (%x), (%s), prevfiles: (%s), findathash: (%s), md5hex: (%x)", pRes->GetFileName(), pRes->GetFileHash(), hash, typeFind, m_PrevHash, pSenderClient->GetName(), prevHashFoundFile, hashFoundFile, swap_endian(hash));
		}

		bHandled = true;
	}

	m_PrevHash = hash;
	AddFileResponse(pSenderClient, resource->szFileName, hash);
	return !bHandled;
}

const char *DuplicateString(const char *str)
{
	for (auto iter = StringsCache.cbegin(), end = StringsCache.cend(); iter != end; ++iter)
	{
		if (!strcmp(*iter, str))
			return *iter;
	}

	const char *s = strcpy(new char[strlen(str) + 1], str);
	StringsCache.push_back(s);
	return s;
}

void ClearStringsCache()
{
	for (auto iter = StringsCache.begin(), end = StringsCache.end(); iter != end; ++iter)
		delete [] *iter;

	StringsCache.clear();
}

CResourceBuffer::CResourceBuffer(char *filename, char *cmdExec, flag_type_resources flag, uint32 hash, int line, bool bBreak)
{
	m_FileName = DuplicateString(filename);
	m_CmdExec = (cmdExec[0] != '\0') ? DuplicateString(cmdExec) : NULL;

	m_Duplicate = false;

	m_Flag = flag;
	m_FileHash = hash;
	m_Line = line;
	m_Break = bBreak;
}

CResourceFile::ResponseBuffer::ResponseBuffer(IGameClient *pSenderClient, char *filename, uint32 hash)
{
	m_pClient = pSenderClient;
	m_FileName = DuplicateString(filename);
	m_ClientHash = hash;
}

const char *CResourceFile::FindFilenameOfHash(uint32 hash)
{
	for (auto iter = m_responseList.begin(), end = m_responseList.end(); iter != end; ++iter)
	{
		if ((*iter)->GetClientHash() == hash)
			return (*iter)->GetFileName();
	}

	return NULL;
}

