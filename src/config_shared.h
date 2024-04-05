#ifndef CONFIG_SHARED_H
#define CONFIG_SHARED_H

#include "layer_utl.h"
#include "struct_utl.h"
#include "ini_parser.h"

struct ConfigLoader
{
	HashArrayMap<IniParserLine, IniParserLine> *CurrentSection;
	SubStr CurrentSectionName = "[root]";
	void *CurrentSectionPointer = nullptr;
	HashMap<const char*, void*> parsedSecions;
	HashMap<const char*, GrowArray<void**>> forwardSections;
	IniParser &parser;
	ConfigLoader(IniParser &p) : CurrentSection(&p.mDict["[root]"]), parsedSecions(), parser(p) {}
	void SetIndex(int idx){}
	void End(size_t size){}
};



struct SectionHeader_
{
	SubStr name = {nullptr, nullptr};
	SectionHeader_(){}
	SectionHeader_(const SectionHeader_ &other) = delete;
	SectionHeader_& operator=(const SectionHeader_ &) = delete;
	SectionHeader_(ConfigLoader &l) : name(l.CurrentSectionName){
	}
};
#define SectionHeader(PREFIX) \
constexpr static const char prefix[] = #PREFIX; \
	SectionHeader_ h;

template <typename S>
struct Sections
{

	HashMap<SubStr, S> mSections;
	Sections(){}
	Sections(const Sections &other) = delete;
	Sections& operator=(const Sections &) = delete;
	Sections(ConfigLoader &l)
	{
		HashArrayMap<IniParserLine, IniParserLine> *PreviousSection = l.CurrentSection;
		void *PrevioisSectionPointer = l.CurrentSectionPointer;
		for(int i = 0; i < l.parser.mDict.TblSize; i++)
		{
			for(auto *node = l.parser.mDict.table[i]; node; node = node->n)
			{
				SubStr s = {node->k + 1, node->k + strlen(node->k) - 1};
				SubStr sp, sn;
				if( s.Split2(sp, sn, '.') && sp.Equals(S::prefix))
				{
					auto *sectionNode = mSections.GetOrAllocate(sn);
					S& section = sectionNode->v;
					l.CurrentSection = &node->v;
					l.CurrentSectionName = sectionNode->k;
					l.parsedSecions[node->k] = (void*)&section;
					l.CurrentSectionPointer = &section;
					ConstructorVisitor<S, ConfigLoader>().Fill(&section,l);
					auto *fwd = l.forwardSections.GetPtr(node->k);
					if(fwd)
					{
						for(int j = 0; j < fwd->count; j++)
						{
							void **sec = (*fwd)[j];
							*sec = (void*)&section;
						}
					}

					l.CurrentSection = PreviousSection;
					l.CurrentSectionPointer = PrevioisSectionPointer;
				}
			}
		}
	}
};


template <typename S, const auto &NAME>
struct SectionReference_
{
	constexpr static const char *name = NAME;
	S *ptr = 0;
	const char *suffix = nullptr;
	SectionReference_(const SectionReference_ &other) = delete;
	SectionReference_& operator=(const SectionReference_ &) = delete;
	SectionReference_(){}
	~SectionReference_()
	{
		if(suffix)
			free((void*)suffix);
		suffix = nullptr;
	}
	SectionReference_(ConfigLoader &l)
	{
		IniParserLine &str = (*l.CurrentSection)[name];
		if(str.begin)
		{
			char sectionName[256];
			SubStr s{str,strchr(str, '.')};
			if(!s.end)
				s.end = s.begin + strlen(str);
			else
				suffix = strdup(s.end + 1);
			SBPrint(sectionName, "[%s.%s]", S::prefix, s);
			auto *n = l.parser.mDict.GetNode(sectionName);
			if(!n)
			{
				Log("Section %s, key %s: missing config section referenced: %s!\n", ((SectionHeader_*)l.CurrentSectionPointer)->name, str, sectionName);
				return;
			}
			ptr = (S*)l.parsedSecions[n->k];
			if(!ptr)
				l.forwardSections[n->k].Add((void**)&ptr);
		}
		str = {nullptr, nullptr};
	}
};
#define SectionReference(type,name) \
constexpr static const char *opt_name_##name = #name; \
	SectionReference_<type, opt_name_##name> name;


template <typename T, const auto &NAME>
struct Option_
{
	constexpr static const char *name = NAME;
	T val;
	operator T() const
	{
		return val;
	}
	Option_(const Option_ &other) = delete;
	Option_& operator=(const Option_ &) = delete;
	Option_(){}
	Option_(const T &def):val(def){}
	Option_(ConfigLoader &l){
		IniParserLine &str = (*l.CurrentSection)[name];
		if(str.begin)
			val = (T)atof(str);
		str ={nullptr, nullptr};
	}
};
#define Option(type,name) \
constexpr static const char *opt_name_##name = #name; \
	Option_<type, opt_name_##name> name;



template <const auto &NAME>
struct StringOption_
{
	constexpr static const char *name = NAME;
	const char *val = nullptr;
	operator const char *()
	{
		return val;
	}
	StringOption_(const StringOption_ &other) = delete;
	StringOption_& operator=(const StringOption_ &) = delete;
	StringOption_(){}
	~StringOption_()
	{
		if(val)
			free((void*)val);
		val = nullptr;
	}
	StringOption_(ConfigLoader &l){
		IniParserLine &str = (*l.CurrentSection)[name];
		if(str.begin)
			val = strdup(str);
		str = {nullptr, nullptr};
	}
};

#define StringOption(name) \
constexpr static const char *opt_name_##name = #name; \
	StringOption_<opt_name_##name> name;

#define IsDelim(c) (!(c) || ((c) == ' '|| (c) == ','))
static size_t GetEnum(const char *scheme, const char *val)
{
	size_t index = 0;
	while(true)
	{
		const char *pval = val;
		char c;
		while((c = *scheme))
		{
			if(!IsDelim(c))
			{
				index++;
				break;
			}
			scheme++;
		}
		while(*scheme && *scheme == *pval)scheme++, pval++;
		if(IsDelim(*scheme) && !*pval)
			return index;
		if(!*scheme)
			return 0;
		while(!IsDelim(*scheme))
			scheme++;
	}
}


template <typename T, const auto &NAME, const auto &SCHEME>
struct EnumOption_
{
	constexpr static const char *name = NAME;
	T val;
	operator T() const
	{
		return val;
	}
	EnumOption_(const EnumOption_ &other) = delete;
	EnumOption_& operator=(const EnumOption_ &) = delete;
	EnumOption_(){}
	EnumOption_(ConfigLoader &l){
		IniParserLine &str = (*l.CurrentSection)[name];
		if(str.begin)
		{
			val = (T)GetEnum(SCHEME, str);
		}
		else
			val = (T)0;
		str = {nullptr, nullptr};
	}
};

#define EnumOption(name, ...) \
enum name ## _enum { \
				  name ## _none, \
				  __VA_ARGS__, \
				  name ## _count \
}; \
	constexpr static const char *name ## _name = #name; \
	constexpr static const char *name ## _scheme = #__VA_ARGS__; \
	EnumOption_<name ## _enum, name ## _name, name ## _scheme> name;

struct SourceSection
{
	SectionHeader(source);
	EnumOption(actionType, action_bool, action_float, action_vector2, action_external);
	// todo: bindings=/user/hand/left/input/grip/pose,/interaction_profiles/valve/index_controller:/user/hand/left/input/select/click
	StringOption(bindings);
	StringOption(subactionOverride);
};

struct BindingProfileSection;
struct ActionMapSection
{
	SectionHeader(actionmap);
	Option(bool, override);
	// TODO: RPN or something similar
	StringOption(axis1);
	StringOption(axis2);
	SectionReference(SourceSection, map);
	EnumOption(customAction, reloadSettings, changeProfile, triggerInteractionChange);
	SectionReference(BindingProfileSection, profileName);
};

static constexpr const char *const gszUserSuffixes[] =
	{
		"left",
		"right",
		"head",
		"gamepad",
		//"/user/treadmill"
};


enum eUserPaths{
	USER_HAND_LEFT = 0,
	USER_HAND_RIGHT,
	USER_HEAD,
	USER_GAMEPAD,
	USER_INVALID,
	USER_PATH_COUNT
};

static int PathIndexFromSuffix(const SubStr &suffix)
{
	int i;
	SubStr s1, s2;
	if(!suffix.Split2(s1, s2, '['))
		s1 = suffix;
	else if(s2.Len() > 2)
		return USER_INVALID;
	for(i = 0; i < USER_INVALID; i++)
		if(s1.Equals(SubStr(gszUserSuffixes[i],strlen(gszUserSuffixes[i]))))
			break;
	return i;
}

struct BindingProfileSection
{
	SectionHeader(bindings);
	struct DynamicActionMaps {
		HashMap<const char *, ActionMapSection*> maps[USER_PATH_COUNT];
		DynamicActionMaps(){}
		DynamicActionMaps(const DynamicActionMaps &other) = delete;
		DynamicActionMaps& operator=(const DynamicActionMaps &) = delete;
		DynamicActionMaps(ConfigLoader &l)
		{
			for(int i = 0; i < l.CurrentSection->TblSize; i++)
			{
				for(int j = 0; j < l.CurrentSection->table[i].count; j++)
				{
					char sectionName[256];
					if(!l.CurrentSection->table[i][j].v)
						continue;
					SBPrint(sectionName, "[%s.%s]", ActionMapSection::prefix, l.CurrentSection->table[i][j].v.begin );
					auto *n = l.parser.mDict.GetNode(sectionName);
					if(!n)
					{
						Log("Section %s, actionmap %s: missing config section referenced: %s\n", ((SectionHeader_*)l.CurrentSectionPointer)->name, l.CurrentSection->table[i][j].k, sectionName);
						continue;
					}

					ActionMapSection *map = (ActionMapSection *)l.parsedSecions[n->k];
					if(!map)
						continue;
					SubStr kk = SubStr(l.CurrentSection->table[i][j].k, strlen(l.CurrentSection->table[i][j].k));
					SubStr nn, s;
					if(kk.Split2(nn, s, '.'))
					{
						char str[nn.Len() + 1];
						nn.CopyTo(str, nn.Len() + 1);
						maps[PathIndexFromSuffix(s)][str] = map;
					}
					else
					{
						maps[USER_HAND_LEFT][l.CurrentSection->table[i][j].k] = map;
						maps[USER_HAND_RIGHT][l.CurrentSection->table[i][j].k] = map;
						maps[USER_INVALID][l.CurrentSection->table[i][j].k] = map;
					}

					l.CurrentSection->table[i][j].v = {nullptr, nullptr};
				}
			}
		}
	} actionMaps;
};
struct Config
{
	// SectionHeader name
	Config(const Config &other) = delete;
	Config& operator=(const Config &) = delete;
	Option(int, serverPort);
	StringOption(interactionProfile);
	Sections<SourceSection> sources;
	Sections<ActionMapSection> actionMaps;
	Sections<BindingProfileSection> bindings;
	SectionReference(BindingProfileSection,startupProfile);
};

static void LoadConfig(Config *c)
{
	const char *cfg = "layer_config.ini";
	IniParser p(cfg);
	if(!p)
	{
		Log("Cannot load %s", cfg);
		return;
	}
	ConfigLoader t = ConfigLoader(p);
	t.CurrentSectionPointer = c;
	ConstructorVisitor<Config, ConfigLoader>().Fill(c,t);
	for(int i = 0; i < p.mDict.TblSize; i++)
		for(auto *node = p.mDict.table[i]; node; node = node->n)
			for(int j = 0; j < node->v.TblSize; j++)
				for(int k = 0; k < node->v.table[j].count; k++ )
					if(node->v.table[j][k].v.begin)
						Log("Section %s: unused config key %s = %s\n", node->k, node->v.table[j][k].k.begin, node->v.table[j][k].v.begin);

	Log("ServerPort %d\n", (int)c->serverPort);
}
#endif // CONFIG_SHARED_H