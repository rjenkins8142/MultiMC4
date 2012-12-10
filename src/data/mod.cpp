// 
//  Copyright 2012 MultiMC Contributors
// 
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
// 
//        http://www.apache.org/licenses/LICENSE-2.0
// 
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//

#include "mod.h"

#define READ_MODINFO

#ifdef READ_MODINFO
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <wx/sstream.h>
#include <wx/dir.h>

#include <string>
#include <sstream>

#include <memory>

#include "utils/apputils.h"

#endif

Mod::Mod(const wxFileName& file, ModType type)
{
	modFile = file;
	modName = modFile.GetName();

	if (type == MOD_UNKNOWN)
	{
		if (wxDirExists(modFile.GetFullPath()))
			type = MOD_FOLDER;
		else if (wxFileExists(modFile.GetFullPath()))
		{
			wxString ext = modFile.GetExt().Lower();
			if (ext == "zip" || ext == "jar")
				type = MOD_ZIPFILE;
			else
				type = MOD_SINGLEFILE;
		}
	}

	modType = type;

#ifdef READ_MODINFO
	switch (modType)
	{
	case MOD_ZIPFILE:
		{
			wxFFileInputStream fileIn(modFile.GetFullPath());
			wxZipInputStream zipIn(fileIn);

			std::auto_ptr<wxZipEntry> entry;

			bool is_forge = false;
			while(true)
			{
				entry.reset(zipIn.GetNextEntry());
				if (entry.get() == nullptr)
					break;
				if(entry->GetInternalName().EndsWith("mcmod.info"))
					break;
				if(entry->GetInternalName().EndsWith("forgeversion.properties"))
				{
					is_forge = true;
					break;
				}
			}

			if (entry.get() != nullptr)
			{
				// Read the info file into text
				wxString infoFileData;
				wxStringOutputStream stringOut(&infoFileData);
				zipIn.Read(stringOut);
				if(!is_forge)
					ReadModInfoData(infoFileData);
				else
					ReadForgeInfoData(infoFileData);
			}
		}
		break;

	case MOD_FOLDER:
		{
			wxString infoFile = Path::Combine(modFile, "mcmod.info");
			if (!wxFileExists(infoFile))
			{
				infoFile = wxEmptyString;

				wxDir modDir(modFile.GetFullPath());

				if (!modDir.IsOpened())
				{
					wxLogError(_("Can't fine mod info file. Failed to open mod folder."));
					break;
				}

				wxString currentFile;
				if (modDir.GetFirst(&currentFile))
				{
					do 
					{
						if (currentFile.EndsWith("mcmod.info"))
						{
							infoFile = Path::Combine(modFile.GetFullPath(), currentFile);
							break;
						}
					} while (modDir.GetNext(&currentFile));
				}
			}

			if (infoFile != wxEmptyString && wxFileExists(infoFile))
			{
				wxString infoStr;
				wxFFileInputStream fileIn(infoFile);
				wxStringOutputStream strOut(&infoStr);
				fileIn.Read(strOut);
				ReadModInfoData(infoStr);
			}
		}
		break;
	}
#endif
}

void Mod::ReadModInfoData(wxString info)
{
	using namespace boost::property_tree;

	// Read the data
	ptree ptRoot;

	std::stringstream stringIn(cStr(info));
	try
	{
		read_json(stringIn, ptRoot);

		ptree pt = ptRoot.get_child("").begin()->second;

		modID = wxStr(pt.get<std::string>("modid"));
		modName = wxStr(pt.get<std::string>("name"));
		modVersion = wxStr(pt.get<std::string>("version"));
	}
	catch (json_parser_error e)
	{
		// Silently fail...
	}
	catch (ptree_error e)
	{
		// Silently fail...
	}
}

void Mod::ReadForgeInfoData ( wxString infoFileData )
{
		using namespace boost::property_tree;

	// Read the data
	ptree ptRoot;
	modName = "Minecraft Forge";
	modID = "Forge";
	std::stringstream stringIn(cStr(infoFileData));
	try
	{
		read_ini(stringIn, ptRoot);
		wxString major, minor, revision, build;
		// BUG: boost property tree is bad. won't let us get a key with dots in it
		// Likely cause = treating the dots as path separators.
		for (auto iter = ptRoot.begin(); iter != ptRoot.end(); iter++)
		{
			auto &item = *iter;
			std::string key = item.first;
			std::string value = item.second.get_value<std::string>();
			if(key == "forge.major.number")
				major = value;
			if(key == "forge.minor.number")
				minor = value;
			if(key == "forge.revision.number")
				revision = value;
			if(key == "forge.build.number")
				build = value;
		}
		modVersion.Empty();
		modVersion << major << "." << minor << "." << revision << "." << build;
	}
	catch (json_parser_error e)
	{
		std::cerr << e.what();
	}
	catch (ptree_error e)
	{
		std::cerr << e.what();
	}
}


Mod::Mod(const Mod& mod)
{
	modFile = mod.GetFileName();
	modName = mod.GetName();
	modVersion = mod.GetModVersion();
	mcVersion = mod.GetMCVersion();
	modType = mod.GetModType();
}

wxFileName Mod::GetFileName() const
{
	return modFile;
}

wxString Mod::GetName() const
{
	return modName;
}

wxString Mod::GetModID() const
{
	return GetFileName().GetFullName();
}

wxString Mod::GetModVersion() const
{
	return modVersion;
}

wxString Mod::GetMCVersion() const
{
	return mcVersion;
}

Mod::ModType Mod::GetModType() const
{
	return modType;
}

bool Mod::IsZipMod() const
{
	return GetModType() == MOD_ZIPFILE;
}

bool Mod::operator ==(const Mod &other) const
{
	return GetFileName().SameAs(other.GetFileName());
}
