#include "stdafx.h"
#include "SpriteArt.h"
#include "SpriteId.h"

#include <cstring>

namespace
{
	std::array<float, 4> Rgb(int32 r, int32 g, int32 b)
	{
		return { static_cast<float>(r) / 255.f, static_cast<float>(g) / 255.f, static_cast<float>(b) / 255.f, 1.f };
	}

	void SetArt(std::vector<SpriteArt>* arts, int32 sprite, const std::vector<std::vector<const char*>>& frames)
	{
		SpriteArt& art = (*arts)[static_cast<size_t>(sprite)];
		art.frames = frames;
		art.height = static_cast<int32>(frames.front().size());
		art.width = static_cast<int32>(std::strlen(frames.front().front()));
	}
}

std::array<float, 4> GetPaletteColor(char key)
{
	switch (key)
	{
		case 'k':
		{
			return Rgb(26, 20, 16);
		}
		case 'a':
		{
			return Rgb(176, 180, 188);
		}
		case 'A':
		{
			return Rgb(112, 116, 126);
		}
		case 'd':
		{
			return Rgb(60, 62, 70);
		}
		case 'R':
		{
			return Rgb(130, 30, 28);
		}
		case 'y':
		{
			return Rgb(226, 184, 72);
		}
		case 'Y':
		{
			return Rgb(150, 112, 40);
		}
		case 'b':
		{
			return Rgb(122, 84, 50);
		}
		case 'B':
		{
			return Rgb(78, 52, 32);
		}
		case 's':
		{
			return Rgb(206, 160, 120);
		}
		case 'g':
		{
			return Rgb(118, 140, 94);
		}
		case 'G':
		{
			return Rgb(72, 90, 60);
		}
		case 'e':
		{
			return Rgb(255, 70, 40);
		}
		case 'z':
		{
			return Rgb(96, 82, 66);
		}
		case 'w':
		{
			return Rgb(232, 224, 200);
		}
		case 'W':
		{
			return Rgb(166, 156, 132);
		}
		case 'p':
		{
			return Rgb(172, 86, 186);
		}
		case 'P':
		{
			return Rgb(112, 50, 126);
		}
		case 'm':
		{
			return Rgb(66, 26, 78);
		}
		case 'o':
		{
			return Rgb(255, 152, 42);
		}
		case 'O':
		{
			return Rgb(255, 228, 132);
		}
		case 'i':
		{
			return Rgb(238, 240, 248);
		}
		case 'I':
		{
			return Rgb(170, 176, 190);
		}
		case 'F':
		{
			return Rgb(62, 190, 104);
		}
		case 'E':
		{
			return Rgb(136, 255, 166);
		}
		case 'v':
		{
			return Rgb(246, 246, 252);
		}
		case 'V':
		{
			return Rgb(192, 194, 214);
		}
		case 'j':
		{
			return Rgb(104, 172, 255);
		}
		case 'J':
		{
			return Rgb(48, 98, 212);
		}
		case '#':
		{
			return Rgb(255, 255, 255);
		}
		case 'l':
		{
			return Rgb(104, 138, 62);
		}
		case 'L':
		{
			return Rgb(64, 94, 40);
		}
		case 'f':
		{
			return Rgb(38, 60, 28);
		}
		case 't':
		{
			return Rgb(144, 108, 68);
		}
		case 'T':
		{
			return Rgb(98, 72, 46);
		}
		case 'c':
		{
			return Rgb(168, 150, 118);
		}
		case 'C':
		{
			return Rgb(118, 102, 80);
		}
		case 'x':
		{
			return Rgb(78, 66, 52);
		}
		case 'q':
		{
			return Rgb(178, 156, 92);
		}
		case 'Q':
		{
			return Rgb(122, 106, 60);
		}
		case 'u':
		{
			return Rgb(92, 98, 54);
		}
		case 'n':
		{
			return Rgb(100, 124, 68);
		}
		case 'N':
		{
			return Rgb(64, 82, 44);
		}
		default:
		{
			return { 0.f, 0.f, 0.f, 0.f };
		}
	}
}

std::vector<SpriteArt> MakeSpriteArts()
{
	std::vector<SpriteArt> arts(static_cast<size_t>(SPRITE_COUNT));

	SetArt(&arts, SPRITE_PLAYER,
		{
			{
				"....kkkkk....",
				"...kaaaaak...",
				"...kaAAAak...",
				"...kAkkkAk...",
				"...kAAAAAk...",
				"....kdddk....",
				"..kkaaaaakk..",
				".kRkaAAAakak.",
				".kRkaAyAakak.",
				".kRkaAAAaksk.",
				".kRkdaaadkk..",
				".kRkbbybbk...",
				".kRkdAAAdk...",
				".kRRkAAAk....",
				"..kRkAkAk....",
				"...kkAkAk....",
				"....kAkAk....",
				"....kAkAk....",
				"....kdkdk....",
				"...kddkddk...",
				"...kkk.kkk...",
			},
			{
				"....kkkkk....",
				"...kaaaaak...",
				"...kaAAAak...",
				"...kAkkkAk...",
				"...kAAAAAk...",
				"....kdddk....",
				"..kkaaaaakk..",
				".kRkaAAAakak.",
				".kRkaAyAakak.",
				".kRkaAAAaksk.",
				".kRkdaaadkk..",
				".kRkbbybbk...",
				".kRkdAAAdk...",
				".kRRkAAAk....",
				"..kRkAkAk....",
				"...kAk.kAk...",
				"..kAk...kAk..",
				"..kAk...kAk..",
				"..kdk...kdk..",
				".kddk...kddk.",
				".kkk.....kkk.",
			},
		});

	SetArt(&arts, SPRITE_ZOMBIE,
		{
			{
				"...kkkk.....",
				"..kggggk....",
				"..kgGegk....",
				"..kggGgk....",
				"...kGGk.....",
				"..kzzzzkkk..",
				".kzzgzzzggk.",
				".kzzzzzk.kgk",
				".kzGzzzk....",
				"..kzzzzk....",
				"..kzzGzk....",
				"..kzzzzk....",
				"..kGGGGk....",
				"..kGk.kGk...",
				"..kGk.kGk...",
				"..kgk.kgk...",
				"..kGk.kGk...",
				".kGGk.kGGk..",
				".kkk..kkk...",
			},
			{
				"...kkkk.....",
				"..kggggk....",
				"..kgGegk....",
				"..kggGgk....",
				"...kGGk.....",
				"..kzzzzkkk..",
				".kzzgzzzggk.",
				".kzzzzzk.kgk",
				".kzGzzzk....",
				"..kzzzzk....",
				"..kzzGzk....",
				"..kzzzzk....",
				"..kGGGGk....",
				"..kGk.kGk...",
				".kGk...kGk..",
				".kgk...kgk..",
				".kGk...kGk..",
				"kGGk...kGGk.",
				"kkk.....kkk.",
			},
		});

	SetArt(&arts, SPRITE_SKELETON,
		{
			{
				"...kkkk....",
				"..kwwwwk...",
				"..kwewek...",
				"..kwwwwk...",
				"...kWkWk...",
				"....kwk....",
				"..kkwwwkk..",
				".kwkWwWkwk.",
				".kwkwWwkwk.",
				".kwkWwWkwk.",
				".kWk.w.kWk.",
				".kwk.w.kwk.",
				"..k.kWk.k..",
				"...kWwWk...",
				"...kw.wk...",
				"...kw.wk...",
				"...kW.Wk...",
				"...kw.wk...",
				"..kww.wwk..",
				"..kkk.kkk..",
			},
			{
				"...kkkk....",
				"..kwwwwk...",
				"..kwewek...",
				"..kwwwwk...",
				"...kWkWk...",
				"....kwk....",
				"..kkwwwkk..",
				".kwkWwWkwk.",
				".kwkwWwkwk.",
				".kwkWwWkwk.",
				".kWk.w.kWk.",
				".kwk.w.kwk.",
				"..k.kWk.k..",
				"...kWwWk...",
				"...kw.wk...",
				"..kw...wk..",
				"..kW...Wk..",
				"..kw...wk..",
				".kww...wwk.",
				".kkk...kkk.",
			},
		});

	SetArt(&arts, SPRITE_BOSS,
		{
			{
				".............kk.......",
				"............kPk.......",
				"......kkk..kPk........",
				".....kpppkkppk........",
				"....kppPPppppkk.......",
				"...kppPmmPppepk.......",
				"..kppPmmmmPpppk.......",
				"..kpPmmmmmmPppkkk.....",
				".kppmmmmmmmmPppppk....",
				".kpPmmmmmmmmmPpPPk....",
				".kpPmmmmmmmmmmPk.kk...",
				"kppPmmmmmmmmmPpk.kpk..",
				"kpPmmmmmmmmmPppkkppk..",
				"kpPmmmmmmmmPpppppPk...",
				".kPmmmmmmmPPkkkkkk....",
				".kPPmmmmmPPk..........",
				"..kPPPPPPPk...........",
				"..kPk.kPPk............",
				"..kPk..kPk............",
				".kPPk..kPPk...........",
				".kmmk..kmmk...........",
				".kkk...kkk............",
			},
			{
				".............kk.......",
				"............kPk.......",
				"......kkk..kPk........",
				".....kpppkkppk........",
				"....kppPPppppkk.......",
				"...kppPmmPppepk.......",
				"..kppPmmmmPpppk.......",
				"..kpPmmmmmmPppkkk.....",
				".kppmmmmmmmmPppppk....",
				".kpPmmmmmmmmmPpPPk....",
				".kpPmmmmmmmmmmPk.kk...",
				"kppPmmmmmmmmmPpk.kpk..",
				"kpPmmmmmmmmmPppkkppk..",
				"kpPmmmmmmmmPpppppPk...",
				".kPmmmmmmmPPkkkkkk....",
				".kPPmmmmmPPk..........",
				"..kPPPPPPPk...........",
				"..kPk..kPk............",
				".kPk....kPk...........",
				".kPk....kPPk..........",
				"kmmk....kmmk..........",
				"kkk......kkk..........",
			},
		});

	SetArt(&arts, SPRITE_SWORD,
		{
			{
				"..i..",
				".iIi.",
				".iIi.",
				".iIi.",
				".iIi.",
				".iIi.",
				".iIi.",
				".iIi.",
				".iIi.",
				"yYyYy",
				"..b..",
				"..B..",
				"..y..",
			},
		});

	SetArt(&arts, SPRITE_GOLD,
		{
			{
				"..yy..",
				".yYYy.",
				"yYyyYy",
				"yyYYyy",
				".YYYY.",
			},
		});

	SetArt(&arts, SPRITE_EXP_ORB,
		{
			{
				".FEF.",
				"FEEEF",
				"EE#EE",
				"FEEEF",
				".FEF.",
			},
		});

	SetArt(&arts, SPRITE_DUST,
		{
			{
				"..v..",
				".vVv.",
				"vV#Vv",
				".vVv.",
				"..v..",
			},
		});

	SetArt(&arts, SPRITE_FRAGMENT,
		{
			{
				"..j..",
				".jJj.",
				"jJ#Jj",
				".jJj.",
				"..j..",
			},
		});

	SetArt(&arts, SPRITE_CORE,
		{
			{
				"..o..",
				".oOo.",
				"oO#Oo",
				".oOo.",
				"..o..",
			},
		});

	SetArt(&arts, SPRITE_PALM,
		{
			{
				"........ll.........",
				"...ll..lLLl..ll....",
				"..lLLl.lLLl.lLLl...",
				".lLffLllLLllLffLl..",
				"lLf..fLLLLLLf..fLl.",
				"lf....fLLTLf....fl.",
				"l.....lLfTfLl.....l",
				"......lf.T.fl......",
				".....lf..T..fl.....",
				"....lf...t...fl....",
				"....f....tT...f....",
				".........tT........",
				".........tT........",
				"........ktT........",
				"........tTk........",
				"........tT.........",
				"........tTk........",
				"........ktT........",
				"........tT.........",
				"........tTk........",
				"........ktT........",
				"........tT.........",
				"........tTk........",
				".......ktT.........",
				".......tTk.........",
				".......tT..........",
				".......tTk.........",
				".......ktT.........",
				".......tT..........",
				".......tTk.........",
				"......ktTT.........",
				"......kttTk........",
				".....kttTTTk.......",
			},
		});

	SetArt(&arts, SPRITE_ROCK,
		{
			{
				"...kkkk....",
				"..kcccCkk..",
				".kccccCCCk.",
				"kcccCCCCxCk",
				"kCCCCCxxxxk",
				"kxCCxxxxxxk",
				".kkkkkkkkk.",
			},
		});

	SetArt(&arts, SPRITE_PEBBLE,
		{
			{
				".kkkk..",
				"kccCCk.",
				"kcCCxxk",
				"kCxxxxk",
				".kkkkk.",
			},
		});

	SetArt(&arts, SPRITE_BUSH,
		{
			{
				"..q...Q..q.",
				".qQ..qQ.qQ.",
				"..qQ.QqqQ..",
				".Q.qQQuQ.q.",
				"..QquuuQq..",
				".QuuuQuuuQ.",
				"..uuuuuuu..",
			},
		});

	SetArt(&arts, SPRITE_GRASS,
		{
			{
				".q..q..",
				"..qQ.q.",
				".QqQq..",
				"..uQu..",
				".uuuuu.",
			},
		});

	SetArt(&arts, SPRITE_REEDS,
		{
			{
				".n...n.",
				".N.n.N.",
				".n.N.n.",
				"nN.n.Nn",
				"Nn.N.nN",
				".N.n.N.",
				".n.N.n.",
				"nN.n.Nn",
				".NnNnN.",
				".nNnNn.",
				"NNnNnNN",
				".NNNNN.",
			},
		});

	SetArt(&arts, SPRITE_BRAZIER,
		{
			{
				"...O...",
				"..OoO..",
				".oOOoo.",
				".ooOoo.",
				"kYyyyYk",
				".kYYYk.",
				"..kAk..",
				"..kAk..",
				".kAAAk.",
				"kkkkkkk",
			},
		});

	SetArt(&arts, SPRITE_BONES,
		{
			{
				"w.....w",
				".wWWWw.",
				"w.....w",
			},
		});

	return arts;
}
