#define THEORY_IMPL
#define THEORY_DUCKTAPE
#include "theory/theory.hh" // https://github.com/SDmodding/TheoryEngine/

#include <map>

using namespace UFG;

int main(int argc, char** argv)
{
	qInit();

	if (2 > argc)
	{
		qPrintf("ERROR: Missing argument for file input.\nUsage: <filename> <resourcename:optional>");
		return 1;
	}

	qString filename = argv[1];
	qString resoucename = (argc > 2 ? argv[2] : filename.GetFilenameWithoutExtension());

	/* Create Map */

	std::map<u32, const char*> map;
	u32 stringsLen = 0;

	auto symbols = (char*)StreamFileWrapper::ReadEntireFile(filename);
	u32 line = 0;
	for (char* sym = symbols; sym;)
	{
		++line;
		auto newline = strchr(sym, '\n');
		if (newline)
		{
			if (newline[-1] == '\r') {
				newline[-1] = 0;
			}
			*newline = 0;
		}

		u32 uid = -1;
		const char* text;

		if (sym[0] == '0' && sym[1] == 'x')
		{
			uid = static_cast<u32>(strtoul(sym, 0, 16));
			text = &sym[11];

			if (qStringHash32(text) != uid && qStringHashUpper32(text) != uid)
			{
				qPrintf("WARN: Ignoring %s (-0x%08X-) at line %u due to hash mismatch!\n", text, uid, line);
				uid = -1;
			}
		}
		else
		{
			uid = qStringHash32(sym);
			text = sym;
		}

		if (uid != -1)
		{
			auto it = map.find(uid);
			if (it == map.end())
			{
				map[uid] = text;
				stringsLen += static_cast<u32>(strlen(text)) + 1;
			}
			else {
				qPrintf("WARN: Ignoring %s (-0x%08X-) at line %u due to hash collision with %s!\n", text, uid, line, (*it).second);
			}
		}

		sym = (newline ? &newline[1] : 0);
	}

	/* Create Symbol Table Resource */

	auto schema = Illusion::GetSchema();

	SymbolTableResource* symbolResource = 0;
	u32 numEntries = static_cast<u32>(map.size());
	char* symbolStrings = 0;

	schema->Init();
	schema->Add("SymbolTableResource", &symbolResource);
	schema->AddArray<SymbolTableEntry>("SymbolTableEntries", numEntries, 0, &symbolResource->mEntries);
	schema->Add("SymbolTableStrings", stringsLen, (void**)&symbolStrings);

	schema->Allocate();

	symbolResource->SetDebugName(resoucename);
	symbolResource->mNode.mUID = qStringHash32(resoucename);

	symbolResource->mTypeUID = RTypeUID_SymbolTableResource;
	symbolResource->mNumEntries = numEntries;

	auto entry = symbolResource->mEntries.Get();

	for (auto& pair : map)
	{
		entry->mUID = pair.first;
		entry->mString.Set(symbolStrings);

		auto len = static_cast<u32>(strlen(pair.second)) + 1;
		memcpy(symbolStrings, pair.second, len);

		symbolStrings += len;

		++entry;
	}

	auto list_filename = filename.GetFilenameWithoutExtension() + "_list.txt";
	if (auto file = qOpen(list_filename, QACCESS_WRITE, 0))
	{
		for (auto& pair : map)
		{
			qString line = { "0x%08X %s\n", pair.first, pair.second };
			qWrite(file, line.mData, line.mLength);
		}

		qClose(file);
	}

	auto bin_filename = filename.GetFilenameWithoutExtension() + ".bin";

	qChunkFileBuilder chunkBuilder;
	chunkBuilder.CreateBuilder("PC64", bin_filename, 0, 0);

	chunkBuilder.BeginChunk(ChunkUID_SymbolTableResource, "UFG.SymbolTable.ChunkV1", 1);
	chunkBuilder.Write(symbolResource, static_cast<u32>(schema->mCurrSize));
	chunkBuilder.EndChunk(ChunkUID_SymbolTableResource);

	chunkBuilder.CloseBuilder(0, true);
}