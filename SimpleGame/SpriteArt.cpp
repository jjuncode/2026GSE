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
			return Rgb(20, 16, 18);
		}
		case 'K':
		{
			return Rgb(44, 38, 40);
		}
		case 's':
		{
			return Rgb(196, 150, 120);
		}
		case 'S':
		{
			return Rgb(140, 100, 80);
		}
		case 't':
		{
			return Rgb(46, 112, 104);
		}
		case 'T':
		{
			return Rgb(26, 70, 68);
		}
		case 'b':
		{
			return Rgb(118, 84, 56);
		}
		case 'B':
		{
			return Rgb(74, 52, 36);
		}
		case 'a':
		{
			return Rgb(176, 180, 188);
		}
		case 'A':
		{
			return Rgb(108, 112, 122);
		}
		case 'd':
		{
			return Rgb(60, 62, 70);
		}
		case 'c':
		{
			return Rgb(160, 152, 136);
		}
		case 'C':
		{
			return Rgb(104, 98, 88);
		}
		case 'x':
		{
			return Rgb(64, 58, 56);
		}
		case 'r':
		{
			return Rgb(150, 40, 36);
		}
		case 'R':
		{
			return Rgb(92, 24, 26);
		}
		case 'e':
		{
			return Rgb(255, 96, 40);
		}
		case 'y':
		{
			return Rgb(222, 180, 84);
		}
		case 'Y':
		{
			return Rgb(142, 106, 46);
		}
		case 'p':
		{
			return Rgb(124, 66, 112);
		}
		case 'P':
		{
			return Rgb(78, 38, 72);
		}
		case 'g':
		{
			return Rgb(98, 114, 74);
		}
		case 'G':
		{
			return Rgb(58, 70, 46);
		}
		case 'h':
		{
			return Rgb(140, 236, 160);
		}
		case 'w':
		{
			return Rgb(124, 96, 64);
		}
		case 'W':
		{
			return Rgb(80, 60, 42);
		}
		case 'o':
		{
			return Rgb(216, 208, 188);
		}
		case 'O':
		{
			return Rgb(150, 142, 124);
		}
		case 'f':
		{
			return Rgb(212, 182, 164);
		}
		case 'F':
		{
			return Rgb(158, 124, 110);
		}
		case 'm':
		{
			return Rgb(104, 50, 50);
		}
		case 'n':
		{
			return Rgb(240, 234, 216);
		}
		case 'u':
		{
			return Rgb(116, 106, 124);
		}
		case 'U':
		{
			return Rgb(72, 64, 84);
		}
		case 'j':
		{
			return Rgb(112, 108, 106);
		}
		case 'J':
		{
			return Rgb(70, 66, 66);
		}
		case 'i':
		{
			return Rgb(166, 160, 152);
		}
		case '1':
		{
			return Rgb(140, 134, 126);
		}
		case '2':
		{
			return Rgb(108, 104, 98);
		}
		case '3':
		{
			return Rgb(80, 76, 74);
		}
		case '4':
		{
			return Rgb(48, 44, 44);
		}
		case '5':
		{
			return Rgb(34, 30, 32);
		}
		case '6':
		{
			return Rgb(112, 60, 46);
		}
		case '7':
		{
			return Rgb(70, 38, 32);
		}
		case '8':
		{
			return Rgb(158, 140, 112);
		}
		case '9':
		{
			return Rgb(114, 98, 80);
		}
		case 'l':
		{
			return Rgb(78, 94, 60);
		}
		case 'v':
		{
			return Rgb(238, 238, 246);
		}
		case 'V':
		{
			return Rgb(176, 178, 196);
		}
		case 'q':
		{
			return Rgb(96, 156, 255);
		}
		case 'Q':
		{
			return Rgb(40, 82, 206);
		}
		case 'z':
		{
			return Rgb(255, 156, 48);
		}
		case 'Z':
		{
			return Rgb(255, 228, 140);
		}
		case '#':
		{
			return Rgb(255, 255, 255);
		}
		case 'E':
		{
			return Rgb(140, 255, 170);
		}
		case 'L':
		{
			return Rgb(60, 190, 110);
		}
		case 'H':
		{
			return Rgb(255, 206, 96);
		}
		case 'I':
		{
			return Rgb(255, 122, 42);
		}
		case 'M':
		{
			return Rgb(186, 52, 22);
		}
		case 'N':
		{
			return Rgb(84, 72, 64);
		}
		case 'X':
		{
			return Rgb(48, 42, 40);
		}
		case 'D':
		{
			return Rgb(128, 114, 76);
		}
		case '0':
		{
			return Rgb(86, 76, 54);
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
				"...kTtttTk...",
				"..kTttttttk..",
				"..kTtkksssk..",
				"..kTkssskSk..",
				"..kTksssssk..",
				"...kTkSSSk...",
				"..kTTkbbbkk..",
				".kTttkbbybak.",
				".kTttkbybbAk.",
				".kTttkaBBsk..",
				".kTttkBBBk...",
				".kTTtkxxxk...",
				"..kTTkxxxk...",
				"...kkkxkxk...",
				"....kxkxk....",
				"....kxkxk....",
				"....kxkxk....",
				"....kBkBk....",
				"...kBBkBBk...",
				"...kkk.kkk...",
			},
			{
				"....kkkkk....",
				"...kTtttTk...",
				"..kTttttttk..",
				"..kTtkksssk..",
				"..kTkssskSk..",
				"..kTksssssk..",
				"...kTkSSSk...",
				"..kTTkbbbkk..",
				".kTttkbbybak.",
				".kTttkbybbAk.",
				".kTttkaBBsk..",
				".kTttkBBBk...",
				".kTTtkxxxk...",
				"..kTTkxxxk...",
				"...kkkxkxk...",
				"....kx.xk....",
				"...kxk.kxk...",
				"..kxk...kxk..",
				"..kBk...kBk..",
				".kBBk...kBBk.",
				".kkk.....kkk.",
			},
		});

	SetArt(&arts, SPRITE_BLIGHTED,
		{
			{
				"...kkkk.....",
				"..kuUuuk....",
				"..kueuuek...",
				"..kuUUuk....",
				"...kmnmk....",
				"..kkUuUkkk..",
				".kTTuuuuuuk.",
				".kTTTuTkkuuk",
				".kTuTTTk..uk",
				".kTTTuTk...k",
				"..kTTTTk....",
				"..kTuTTk....",
				"..kUUUUk....",
				"..kUk.kUk...",
				"..kuk.kuk...",
				"..kUk.kUk...",
				"..kuk.kuk...",
				".kUUk.kUUk..",
				".kkk..kkk...",
			},
			{
				"...kkkk.....",
				"..kuUuuk....",
				"..kueuuek...",
				"..kuUUuk....",
				"...kmnmk....",
				"..kkUuUkkk..",
				".kTTuuuuuuk.",
				".kTTTuTkkuuk",
				".kTuTTTk..uk",
				".kTTTuTk...k",
				"..kTTTTk....",
				"..kTuTTk....",
				"..kUUUUk....",
				"..kUk.kUk...",
				".kUk...kUk..",
				".kuk...kuk..",
				".kUk...kUk..",
				"kUUk...kUUk.",
				"kkk.....kkk.",
			},
		});

	SetArt(&arts, SPRITE_HOUND,
		{
			{
				"..........k.k...",
				".........kJkJk..",
				"k.......kjjjjjk.",
				"kJk....kjjjejjjk",
				".kJkkkkjjjjjiiik",
				"..kjjjjjjjjjkkk.",
				"..kjiijjjjjjk...",
				"..kjjjjjjjJJk...",
				"..kJJk...kJJk...",
				"..kJJk...kJJk...",
				"..kkkk...kkkk...",
			},
			{
				"..........k.k...",
				".........kJkJk..",
				"k.......kjjjjjk.",
				"kJk....kjjjejjjk",
				".kJkkkkjjjjjiiik",
				"..kjjjjjjjjjkkk.",
				"..kjiijjjjjjk...",
				"..kjjjjjjjJJk...",
				".kJkkJk..kJkkJk.",
				"kJk..kJ.kJk..kJk",
				"kk....kkkk....kk",
			},
		});

	SetArt(&arts, SPRITE_DEVOURER,
		{
			{
				"..........kkkkkk......",
				".........kFfffffk.....",
				"........kFFffffffk....",
				".......kFFFffffffk....",
				".......kFFFffkffkfk...",
				".......kFFFfffffffk...",
				".......kFFFfffffffk...",
				".......kFFmfffffffmk..",
				".......kFFFnmnmnmnk...",
				"........kFFmnmnmnmk...",
				".....kkkkFFFFFFFkk....",
				"....kFFFffFFFFfffk....",
				"...kFFFFffFfffffffk...",
				"..kFFFFFffFfffffFffk..",
				"..kFFFFFffffffffFffk..",
				"..kFFFFFffffffffFfffk.",
				".kFFFFFFFFFFFFFFFfffk.",
				".kFFFFFFFFFfffffFFffk.",
				".kFFFFFFFFffffffFFffk.",
				".kFFFFFFFFFfffffFFffk.",
				".kFFFFFFFFfffffffFffk.",
				".kFFFFFFFFFffffffFffk.",
				".kFFFFFFFFfffffffFffk.",
				"kFFFFFFFFFfffffffFfffk",
				"kFFFFFFFFFfffffffFfffk",
				"kFFFFkFFFFFFFFFFkFfffk",
				"kFFFFkFFFFFFFFFfkFfffk",
				"kFFFFkFFFFFFFFffkFfffk",
				".kkkkkFFFFFFFfffkkFFk.",
				".....kFFFFFFFfffk.kk..",
				".....kFFFFFFFfffk.....",
				".....kFFFFFFFfffk.....",
				".....kFFFFkkFfffk.....",
				".....kFFFFkkFfffk.....",
				".....kFFFFkkFfffk.....",
				".....kFFFFkkFfffk.....",
				".....kFFFFkkFfffk.....",
				".....kFFFFkkFfffk.....",
			},
			{
				"..........kkkkkk......",
				".........kFfffffk.....",
				"........kFFffffffk....",
				".......kFFFffffffk....",
				".......kFFFffkffkfk...",
				".......kFFFfffffffk...",
				".......kFFFfffffffk...",
				".......kFFmfffffffmk..",
				".......kFFFnmnmnmnk...",
				"........kFFmnmnmnmk...",
				".....kkkkFFFFFFFkk....",
				"....kFFFffFFFFfffk....",
				"...kFFFFffFfffffffk...",
				"..kFFFFFffFfffffFffk..",
				"..kFFFFFffffffffFffk..",
				".kFFFFFFffffffffFfffk.",
				".kFFFFFFFFFFFFFFFFffk.",
				".kFFFFFFFFFffffffFffk.",
				".kFFFFFFFFfffffffFffk.",
				"kFFFFFFFFFFffffffFfffk",
				"kFFFFFFFFFfffffffFFffk",
				"kFFFkFFFFFFffffffFFffk",
				"kFFFkFFFFFfffffffkFffk",
				"FFFFkFFFFFfffffffkFfff",
				"FFFFkFFFFFfffffffkFfff",
				"FFFFkkFFFFFFFFFFkkFfff",
				"FFFFkkFFFFFFFFFfkkFfff",
				"FFFk.kFFFFFFFFffkkFfff",
				"kkk..kFFFFkkFFffk.kFFk",
				"....kFFFFFkkFFfffk.kk.",
				"....kFFFFFkkFFfffk....",
				"....kFFFFFkkFFfffk....",
				"....kFFFFk..kFfffk....",
				"...kFFFFFk..kFFfffk...",
				"...kFFFFFk..kFFfffk...",
				"...kFFFFFk..kFFfffk...",
				"...kFFFFk....kFfffk...",
				"...kFFFFk....kFfffk...",
			},
		});

	SetArt(&arts, SPRITE_GATEBREAKER,
		{
			{
				"...........kok........kok.........",
				"...........kOok.kkkkkkOok.........",
				"............kookFFFFFOok..........",
				"............kOomFFFFFok...........",
				".............komFFFFFFFk..........",
				"............kmmmFFFFFFFFk.........",
				"............kmmmFFFFFFFFk.........",
				"...........kmmmmFFFeeFFeFk........",
				"...........kmmmmFFFFFFFFFk........",
				"...........kmmmmFFFFFFFFFk........",
				"...........kmmmmFFFFFFFFFk........",
				"............kmmmFFFFFFFFk.........",
				"......kk....kmmmnknknknknkk.......",
				"...kkkookkk..kmmmnmnmnmnmnokkk....",
				"..kOOooooook..kmmmmmmmOOooooook...",
				".kOOOoooooook.kmmmmmmOOOoomFFFok..",
				".kOOOoooooookkkmmFFFFOOOommFFFFk..",
				".kOOOoooooookFFFmFFFkOOOommFFFFk..",
				".kOOOooooooomFFFFFFooOOOommFFFFk..",
				"..kOOOOOOOOmmFFOOoooooOOOmmFFFFk..",
				"..kmmFOOFmmmmFOOOoooooookmmmFFFFk.",
				".kmmmFFFmmmmmOOOOoooooooommmFFFFk.",
				".kmmmFFmmmmmmOOOOoooooooommmFFFFk.",
				".kmmFFFmmmmmmOOOOooooooooFmmFFFFk.",
				".kmmFFFmmmmmmOOOOooooooooFmmFFFFk.",
				".kmmFFmmmmmmmOOOOooooooooFmmFFFFk.",
				".kmmFFmmmmmmmFOOOoooooooFFmmFFFFk.",
				".kmmFFmmmmmRmFFOOOOOOOOFFFmmmFFFFk",
				"kmmmFFmmmmmmRFFFFOOOOFFFFFmmmFFFFk",
				"kmmmFFmmmmmmmRFFFFFFFFFFFFmmmFFFFk",
				"kmmFFFmmmmmmmFRoooooFFFFFFFmmFFFFk",
				"kmmFFFFmmmmmOOOoooooooFFFFkmmFFFFk",
				"kmmFFFFmmmmmOOOoooooooFFFFkmmFFFFk",
				"kmmFFFFmmmmmOOOoooooooFFFFkmmFFFFF",
				"kmmFFFFkmmmmOOOoooooooFFFkkmmFFFFF",
				"kmmFFFFkkmmmmmOOOOOOmmmmk.kmmFFFFF",
				"kmmFFFFkkmmmmmmmmmmmmmmFFkkmmFFFFk",
				"mmmFFFFFmmmmmmmmmmmmmmFFFkmmmFFFFF",
				"mmmFFFFFmmmFFmmmmmmmmFFFFkmmmFFFFF",
				"kmmFFFFkmmmFFFFFkkmmmFFFFkmmmFFFFF",
				"kmmmmmmkmmmFFFFFkkmmmFFFFkkmmFFFFk",
				".kkmmkkkmmmFFFFk.kmmmFFFFk.kmmmmk.",
				"...kk..kmmmFFFFk.kmmmFFFFk..kkkk..",
				".......kmmmFFFFk.kmmmFFFFk........",
				".......kmmmFFFFk.kmmmFFFFk........",
				".......kmmmFFFFk.kmmmFFFFk........",
				".......kmmmFFFFk.kmmmFFFFk........",
				".......kmmmFFFFk.kmmmFFFFk........",
				".......kmmmFFFFk.kmmmFFFFk........",
				".......kmmmFFFFk.kmmmFFFFFk.......",
				".......kmmmFFFFk.kmmmFFFFFk.......",
				".......kmmmFFFFk.kmmmFFFFFk.......",
				".......kmmmFFFFk.kmmmFFFFFk.......",
				".......kmmmFFFFk..kmmFFFFk........",
			},
			{
				"...........kok........kok.........",
				"...........kOok.kkkkkkOok.........",
				"............kookFFFFFOok..........",
				"............kOomFFFFFok...........",
				".............komFFFFFFFk..........",
				"............kmmmFFFFFFFFk.........",
				"............kmmmFFFFFFFFk.........",
				"...........kmmmmFFFeeFFeFk........",
				"...........kmmmmFFFFFFFFFk........",
				"...........kmmmmFFFFFFFFFk........",
				"...........kmmmmFFFFFFFFFk........",
				"............kmmmFFFFFFFFk.........",
				"......kk....kmmmnknknknknkk.......",
				"...kkkookkk..kmmmnmnmnmnmnokkk....",
				"..kOOooooook..kmmmmmmmOOooooook...",
				".kOOOoooooook.kmmmmmmOOOoomFFFok..",
				".kOOOoooooookkkmmFFFFOOOommFFFFk..",
				".kOOOoooooookFFFmFFFkOOOommFFFFk..",
				".kOOOooooooomFFFFFFooOOOommFFFFk..",
				"..kOOOOOOOOmmFFOOoooooOOOmmmFFFFk.",
				".kmmmFOOkmmmmFOOOoooooookmmmFFFFk.",
				".kmmFFFFmmmmmOOOOooooooookmmFFFFk.",
				".kmmFFFmmmmmmOOOOooooooooFmmFFFFk.",
				".kmmFFFmmmmmmOOOOooooooooFmmFFFFFk",
				"kmmmFFFmmmmmmOOOOooooooooFmmmFFFFk",
				"kmmFFFmmmmmmmOOOOooooooooFmmmFFFFk",
				"kmmFFFmmmmmmmFOOOoooooooFFFmmFFFFk",
				"kmmFFFmmmmmRmFFOOOOOOOOFFFFmmFFFFF",
				"mmmFFFmmmmmmRFFFFOOOOFFFFFFmmmFFFF",
				"mmmFFFmmmmmmmRFFFFFFFFFFFFFmmmFFFF",
				"mmFFFFmmmmmmmFRoooooFFFFFFFkmmFFFF",
				"mmFFFFkmmmmmOOOoooooooFFFFkkmmFFFF",
				"mmFFFFkmmmmmOOOoooooooFFFFkkmmmFFF",
				"mmFFFkkmmmmmOOOoooooooFFFFkkmmmFFF",
				"mFFFFk.kmmmmOOOoooooooFFFk..kmmFFF",
				"mFFFFk..kmmmmmOOOOOOmmmmk...kmFFFF",
				"mFFFFFk.kmmmmmmmmmmmmmmFFk.kmmFFFF",
				"mFFFFFkkmmmmmmmmmmmmmmFFFk.kmmFFFF",
				"mFFFFFkkmmmFFmmmmmmmmFFFFk.kmmFFFF",
				"mFFFFFkkmmmFFFFkkkmmmFFFFk.kmmFFFF",
				"mmmmmk.kmmmFFFFk.kmmmFFFFFkkmmFFFF",
				"kkmkk..kmmFFFFFk..kmmFFFFFk.kkmmmk",
				"..k...kmmmFFFFFk..kmmmFFFFk...kkk.",
				"......kmmmFFFFFk..kmmmFFFFk.......",
				"......kmmmFFFFk...kmmmFFFFFk......",
				"......kmmFFFFFk...kmmmFFFFFk......",
				"......kmmFFFFFk....kmmFFFFFk......",
				".....kmmmFFFFFk....kmmmFFFFk......",
				".....kmmmFFFFk.....kmmmFFFFk......",
				".....kmmmFFFFk.....kmmmFFFFFk.....",
				".....kmmFFFFFk.....kmmmFFFFFk.....",
				"....kmmmFFFFFk......kmmFFFFFk.....",
				"....kmmmFFFFFk......kmmmFFFFk.....",
				".....kmmFFFFk.......kmmmFFFFk.....",
			},
		});

	SetArt(&arts, SPRITE_SWORD,
		{
			{
				"..a..",
				".aAa.",
				".aAa.",
				".aAa.",
				".aAa.",
				".aAa.",
				".aAa.",
				".aAa.",
				".aAa.",
				"YyYyY",
				"..W..",
				"..W..",
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
				".LEL.",
				"LEEEL",
				"EE#EE",
				"LEEEL",
				".LEL.",
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
				"..q..",
				".qQq.",
				"qQ#Qq",
				".qQq.",
				"..q..",
			},
		});

	SetArt(&arts, SPRITE_CORE,
		{
			{
				"..z..",
				".zZz.",
				"zZ#Zz",
				".zZz.",
				"..z..",
			},
		});

	SetArt(&arts, SPRITE_DEAD_TREE,
		{
			{
				".....k........kNk....",
				"....kNk......kXk.....",
				"....kXNk.....kNk.....",
				".....kNk....kNkk.....",
				".....kXNk..kXNkNk....",
				"......kNk..kXNkNk..k.",
				"......kXNk.kNkkXk.kNk",
				".....kkXNk.kNkkXNkNk.",
				"..k.kXkkNkkXNk.kNXk..",
				".kNkkNkkXNkXNk.kNNk..",
				".kXkkNkkXNkNk.kXNk...",
				"..kNkNk.kNXNkkXNk....",
				"..kXXk..kXXNkXNNk....",
				"...kNNk.kXNNXNNk.....",
				"...kXNk.kXNXNNk......",
				"....kXNkkXNNNk.......",
				".....kNNkXXNk........",
				".....kXNNXNNk........",
				"......kXNNNNk........",
				".......kXNNNk........",
				"........kXNNk........",
				"........kXNNk........",
				"........kXNNk........",
				"........kXNNk........",
				".......kXXNNk........",
				".......kXXNNk........",
				".......kXXNNk........",
				".......kXXNNk........",
				".......kXXNNk........",
				".......kXXNNk........",
				"......kkXXNNNkk......",
				".....kXXXXNNNNNk.....",
			},
			{
				".............k.......",
				".......k....kXk...k..",
				"...k..kXk....kNk.kNk.",
				"..kNk.kNk....kXkkNk..",
				"..kXkkXk......kXNk...",
				"...kNkNk.....kXNk....",
				"....kXk......kNNk....",
				"....kXNk....kXNk.....",
				".....kNNk...kNk......",
				".....kXNk..kXNk....k.",
				"......kXNkkXNk....kXk",
				".k.....kNNNNNk....kNk",
				"kNk....kXNXNk....kNk.",
				"kNk.....kXNNk..kkXk..",
				"kXNk....kXNNk.kXNNk..",
				".kNk....kXNNkkXNNk...",
				".kXNk...kXNNXNNNk....",
				".kXNNk..kXNXNNkk.....",
				"..kXNNkkkXNNNk.......",
				"...kkNNNkXNNk........",
				".....kXNNXNNk........",
				"......kXNNNNk........",
				".......kXNNNk........",
				"........kXNNk........",
				".......kXXNNk........",
				".......kXXNNk........",
				".......kXXNNk........",
				".......kXXNNk........",
				".......kXXNNk........",
				".......kXXNNk........",
				"......kkXXNNNkk......",
				".....kXXXXNNNNNk.....",
			},
		});

	SetArt(&arts, SPRITE_ROCK,
		{
			{
				"....kkkk...",
				"..kk2211k..",
				".k3322111k.",
				"k333222211k",
				"k333322221k",
				"k533332222k",
				".kkkkkkkkk.",
			},
		});

	SetArt(&arts, SPRITE_PEBBLE,
		{
			{
				".kkkk..",
				"k3221k.",
				"k33221k",
				"k53322k",
				".kkkkk.",
			},
		});

	SetArt(&arts, SPRITE_THORN,
		{
			{
				"..0...D..0.",
				".0D..0D.0D.",
				"..0D.D00D..",
				".D.0DD0D.0.",
				"..D0000D0..",
				".D00X0X00D.",
				"..0XXXXX0..",
			},
		});

	SetArt(&arts, SPRITE_GRASS,
		{
			{
				".D..D..",
				"..D0.D.",
				".0D0D..",
				"..0X0..",
				".0XXX0.",
			},
		});

	SetArt(&arts, SPRITE_REEDS,
		{
			{
				".g...g.",
				".G.g.G.",
				".g.G.g.",
				"gG.g.Gg",
				"Gg.G.gG",
				".G.g.G.",
				".g.G.g.",
				"gG.g.Gg",
				".GgGgG.",
				".gGgGg.",
				"GGgGgGG",
				".GGGGG.",
			},
		});

	SetArt(&arts, SPRITE_BRAZIER,
		{
			{
				"...H...",
				"..HIH..",
				".IHHII.",
				".IIHIM.",
				"kYyyyYk",
				".kYYYk.",
				"..kdk..",
				"..kdk..",
				".kdddk.",
				"kkkkkkk",
			},
		});

	SetArt(&arts, SPRITE_BONES,
		{
			{
				"o.....o",
				".oOOOo.",
				"o.....o",
			},
		});

	SetArt(&arts, SPRITE_NPC_HUNTER,
		{
			{
				".W..kkkk.....",
				"W..kbbbbk....",
				"W.kbBbbbbk...",
				"W.kbkksssk...",
				"WkbBksskSk...",
				"WkiiksssskK..",
				"WkiiikSSk....",
				"kiiiigGgkk...",
				"kjiigGggsk...",
				"WkjgGgGgk....",
				"WkgGgggBk....",
				"W.kbbybbk....",
				".WkGgggGk....",
				"..kGgggGk....",
				"...kkGkGk....",
				"....kBkBk....",
				"....kBkBk....",
				"....kBkBk....",
				"....kKkKk....",
				"...kKKkKKk...",
				"...kkk.kkk...",
			},
			{
				".W..kkkk.....",
				"W..kbbbbk....",
				"W.kbBbbbbk...",
				"W.kbkksssk...",
				"WkbBksskSk...",
				"WkiiksssskK..",
				"WkiiikSSk....",
				"kiiiigGgkk...",
				"kjiigGggsk...",
				"WkjgGgGgk....",
				"WkgGgggBk....",
				"W.kbbybbk....",
				".WkGgggGk....",
				"..kGgggGk....",
				"...kkGkGk....",
				"....kB.Bk....",
				"...kBk.kBk...",
				"..kBk...kBk..",
				"..kKk...kKk..",
				".kKKk...kKKk.",
				".kkk.....kkk.",
			},
		});

	SetArt(&arts, SPRITE_NPC_RAIDER,
		{
			{
				"....kkkk.....",
				"...kKKKKk....",
				"..kKKKKKKk...",
				"..kKksskSk...",
				"..kKrrrrrk...",
				"..kKrRrRrk.a.",
				"...kRrrrk.kak",
				"..kaKkxxkkWk.",
				".kaaKxxxxWk..",
				".kKKxrxxsk...",
				".kKKxxxxk....",
				".kKxBBBBk....",
				".kKxxxxxk....",
				"..kxxxxxk....",
				"...kkxkxk....",
				"....kxkxk....",
				"....kxkxk....",
				"....kxkxk....",
				"....kKkKk....",
				"...kKKkKKk...",
				"...kkk.kkk...",
			},
			{
				"....kkkk.....",
				"...kKKKKk....",
				"..kKKKKKKk...",
				"..kKksskSk...",
				"..kKrrrrrk...",
				"..kKrRrRrk.a.",
				"...kRrrrk.kak",
				"..kaKkxxkkWk.",
				".kaaKxxxxWk..",
				".kKKxrxxsk...",
				".kKKxxxxk....",
				".kKxBBBBk....",
				".kKxxxxxk....",
				"..kxxxxxk....",
				"...kkxkxk....",
				"....kx.xk....",
				"...kxk.kxk...",
				"..kxk...kxk..",
				"..kKk...kKk..",
				".kKKk...kKKk.",
				".kkk.....kkk.",
			},
		});

	SetArt(&arts, SPRITE_NPC_MERCHANT,
		{
			{
				"....kkkkk....",
				"...kYyyyYk...",
				".kkYyyyyyYkk.",
				"kYYYYYYYYYYYk",
				".kkkssskskk..",
				"..wkssssssk..",
				".wWkkSSSSk...",
				"kwWWkpppppk..",
				"kwwWkpPyPpk..",
				"kwWWkpppysk..",
				"kWwWkpPppk...",
				".kWWkyyyyk...",
				"..kkpPpPpk...",
				"...kpPpPpk...",
				"...kkPpPpk...",
				"...kpPpPpk...",
				"...kPpPpPk...",
				"...kpPpPpk...",
				"...kkBkBkk...",
				"....kBkBk....",
				"....kk.kk....",
			},
			{
				"....kkkkk....",
				"...kYyyyYk...",
				".kkYyyyyyYkk.",
				"kYYYYYYYYYYYk",
				".kkkssskskk..",
				"..wkssssssk..",
				".wWkkSSSSk...",
				"kwWWkpppppk..",
				"kwwWkpPyPpk..",
				"kwWWkpppysk..",
				"kWwWkpPppk...",
				".kWWkyyyyk...",
				"..kkpPpPpk...",
				"...kpPpPpk...",
				"...kkPpPpk...",
				"...kpPpPpk...",
				"...kPpPpPk...",
				"..kpPpPpPpk..",
				"..kkBk.kBkk..",
				"..kBk...kBk..",
				"..kk.....kk..",
			},
		});

	SetArt(&arts, SPRITE_NPC_GATHERER,
		{
			{
				"....kkkk.....",
				"...kCccck....",
				"..kCcccccCk..",
				"..kCckssssk..",
				".gkCksskSk...",
				"gGkCkssssk...",
				"kgGgkkSSk....",
				"kwwwwkcccck..",
				"kWwWwkcCcsk..",
				"kwWwWkccck...",
				"kWwWwkcCck...",
				".kWWkkbbbk...",
				"..kkCcccCk...",
				"...kCcccCk...",
				"...kkCkCkk...",
				"....kCkCk....",
				"....kCkCk....",
				"....kCkCk....",
				"....kBkBk....",
				"...kBBkBBk...",
				"...kkk.kkk...",
			},
			{
				"....kkkk.....",
				"...kCccck....",
				"..kCcccccCk..",
				"..kCckssssk..",
				".gkCksskSk...",
				"gGkCkssssk...",
				"kgGgkkSSk....",
				"kwwwwkcccck..",
				"kWwWwkcCcsk..",
				"kwWwWkccck...",
				"kWwWwkcCck...",
				".kWWkkbbbk...",
				"..kkCcccCk...",
				"...kCcccCk...",
				"...kkCkCkk...",
				"....kC.Ck....",
				"...kCk.kCk...",
				"..kCk...kCk..",
				"..kBk...kBk..",
				".kBBk...kBBk.",
				".kkk.....kkk.",
			},
		});

	SetArt(&arts, SPRITE_NPC_WARDEN,
		{
			{
				"....kkkk..kW.",
				"...kaaaAk.kWk",
				"..kaAAAAAk.W.",
				"..kAkksssk.W.",
				"..kTksskSk.W.",
				"..kTkssssk.W.",
				"...kTkSSk..W.",
				"..kTTkAAAkkW.",
				".kTttkAdAAsW.",
				".kTttkdAdAkW.",
				".kTttkAAAk.W.",
				".kTttkyyyk.W.",
				".kTTtkxxxk.W.",
				"..kTTkxxxk.W.",
				"...kkkxkxk.W.",
				"....kxkxk..W.",
				"....kxkxk..W.",
				"....kxkxk..W.",
				"....kBkBk..W.",
				"...kBBkBBk.W.",
				"...kkk.kkk.W.",
			},
			{
				"....kkkk..kW.",
				"...kaaaAk.kWk",
				"..kaAAAAAk.W.",
				"..kAkksssk.W.",
				"..kTksskSk.W.",
				"..kTkssssk.W.",
				"...kTkSSk..W.",
				"..kTTkAAAkkW.",
				".kTttkAdAAsW.",
				".kTttkdAdAkW.",
				".kTttkAAAk.W.",
				".kTttkyyyk.W.",
				".kTTtkxxxk.W.",
				"..kTTkxxxk.W.",
				"...kkkxkxk.W.",
				"....kx.xk..W.",
				"...kxk.kxk.W.",
				"..kxk...kxkW.",
				"..kBk...kBkW.",
				".kBBk...kBBW.",
				".kkk.....kkW.",
			},
		});

	SetArt(&arts, SPRITE_BLOCK_WALL,
		{
			{
				"..............3333..............",
				"............31222223............",
				"..........322222221223..........",
				"........3222211222221223........",
				"......32222221222222222223......",
				"....322122222222122222222123....",
				"..3221222122222222212222221213..",
				"32212222211122122121222222211223",
				"32121222212222112212222212221113",
				"k332221222222211211222221122232k",
				"k333322122222222222221222123222k",
				"k333333222211222222212121322222k",
				"k333335432121222212222132222222k",
				"k433333433311222122213241222224k",
				"k344353435333222122322242222442k",
				"k534443433333333332221242244222k",
				"k334334453533334122122244424212k",
				"k334533344333354222212441224122k",
				"k434333353443334121244112224224k",
				"k344353335344434224422212124442k",
				"k353443333343344442422222244222k",
				"k355334453343333212422224422222k",
				"k333333444343333222422442222222k",
				"k453335433443553221444242122224k",
				"k344333433534433224412242121442k",
				"k534443433333544442221242244212k",
				"k334334435335334221121244424222k",
				"k534335344335334221112442224112k",
				"k434333333443354122244222224124k",
				"k344333333344434224422212224442k",
				"k333443333343344442411222244221k",
				"k333334435543333212412224422222k",
				"k333333444343333112422442222222k",
				"k433333433443333212444242222214k",
				"kk4433343333443322442214222244kk",
				"..kk443433353344442222242244kk..",
				"....kk44333333342222222444kk....",
				"......kk4435335422221244kk......",
				"........kk443334222144kk........",
				"..........kk44342244kk..........",
				"............kk4444kk............",
				"..............kkkk..............",
			},
		});

	SetArt(&arts, SPRITE_BLOCK_TOWER,
		{
			{
				"..............kkkk..............",
				"............k111111k............",
				"..........k1133333311k..........",
				"........k11333333333311k........",
				"......k113333333333333311k......",
				"....k1133333333333333333311k....",
				"..k11333333333333333333333311k..",
				"k113333333333333333333333333311k",
				"k113333333333333333333333333311k",
				"k3k11333333333333333333333311k2k",
				"k333k1133333333333333333311k222k",
				"k33333k113333333333333311k22222k",
				"k3433334k11333333333311k1222222k",
				"k333343443k1133333311k242212222k",
				"k43333343433k111111k22242222114k",
				"k3443334333334kkkk2222242222442k",
				"k334444433333334222122242244122k",
				"k334334434333334222212244424221k",
				"k334334344343344222222442124222k",
				"k334333343443344211244221224222k",
				"k434333k3334443422442222k224214k",
				"k344333kk43433444424222kk224442k",
				"k344443kk33433332224222kk244212k",
				"k333334kk33433332224222kk422222k",
				"k333333kk44433332224224kk222222k",
				"k343433kk34433332224442kk222222k",
				"k434343kk33344332244222kk222214k",
				"k344334kk43333444422112kk222442k",
				"k3444434k33343442222222k1244222k",
				"k334334433343334222222244424222k",
				"k334433344333334222221442124t22k",
				"k3343333334433442222442222ttt22k",
				"k43433344334443422442222ttttt14k",
				"k344333333343344442422ttttttt42k",
				"k4434433343433332224Ttttttttt21k",
				"k333334433343334222TTtttttttt22k",
				"k433333444443333122TTtttttttt22k",
				"k333333433443333222TTttttttt.21k",
				"k433333434334433224TTttttt.t224k",
				"k344344433333344442TTttt.t22442k",
				"k334443433333334222TTt.t2244222k",
				"k334334434333434122T.t244414212k",
				"k334333344333344222T22441124222k",
				"k334333333443434222244212124222k",
				"k434333344344434224422222224224k",
				"k344333333343344442422222224442k",
				"k333443333343333112422112244122k",
				"k343334433343434222422224421112k",
				"kk3333344434333422242244222122kk",
				"..kk334433444343211444242122kk..",
				"....kk34333344332244222422kk....",
				"......kk3333334444122224kk......",
				"........kk333434212222kk........",
				"..........kk33342222kk..........",
				"............kk3422kk............",
				"..............kkkk..............",
			},
		});

	SetArt(&arts, SPRITE_BLOCK_HOUSE,
		{
			{
				"..............kkkk..............",
				"............k666776k............",
				"..........k6667766667k..........",
				"........k66677666677666k........",
				"......k666776666776666776k......",
				"....k6667766667766667766667k....",
				"..k66677666677666677666677666k..",
				"k666776666776666776666776666776k",
				"k677666677666677666677666677666k",
				"kWk66677666677666677666677666kWk",
				"kWWWk7666677666677666677666kWWWk",
				"kW99WWk677666677666677666kWW88Wk",
				"kW9999WWk66677666677666kWW8888Wk",
				"kW999999WWk7666677666kWW888888Wk",
				"kW99999999WWk677666kWW88888888Wk",
				"kW9999999999WWkkkkWW8888888888Wk",
				"kW999999999999WWWW888888888888Wk",
				"kW999999999999WWWW88888888k888Wk",
				"kW999999999999WWWW888888kkk888Wk",
				"kW999999999999WWWW8888kkBWk888Wk",
				"kW999999999999WWWW8888kWWWk888Wk",
				"kW999999999999WWWW8888kBWBk888Wk",
				"k3999999999999WWWW8888kWBWk8882k",
				"k3339999999999WWWW8888kWWWk8222k",
				"kk333399999999WWWW8888kBWB2222kk",
				"..kk3333999999WWWW8888kW2222kk..",
				"....kk33339999WWWW88882222kk....",
				"......kk333399WWWW882222kk......",
				"........kk3333WWWW2222kk........",
				"..........kk33332222kk..........",
				"............kk3322kk............",
				"..............kkkk..............",
			},
			{
				"..............kkkk..............",
				"............k666776k............",
				"..........k6667766667k..........",
				"........k66677666677666k........",
				"......k666776666776666776k......",
				"....k6667766667766667766667k....",
				"..k66677666677666677666677666k..",
				"k666776666776666776666776666776k",
				"k677666677666677666677666677666k",
				"kWk66677666677666677666677666kWk",
				"kWWWk7666677666677666677666kWWWk",
				"kW99WWk677666677666677666kWW88Wk",
				"kW9999WWk66677666677666kWW8888Wk",
				"kW999999WWk7666677666kWW888888Wk",
				"kW99WW9999WWk677666kWW88888888Wk",
				"kW99WHWW9999WWkkkkWW8888WW8888Wk",
				"kW99WWWWWW9999WWWW8888WWHW8888Wk",
				"kW99WHWWHW9999WWWW88WWWWWW8888Wk",
				"kW99WHWWWW9999WWWW88WHWWHW8888Wk",
				"kW99WWWWHW9999WWWW88WWWWHW8888Wk",
				"kW9999WWHW9999WWWW88WHWWWW8888Wk",
				"kW999999WW9999WWWW88WHWW888888Wk",
				"k3999999999999WWWW88WW888888882k",
				"k3339999999999WWWW8888888888222k",
				"kk333399999999WWWW888888882222kk",
				"..kk3333999999WWWW8888882222kk..",
				"....kk33339999WWWW88882222kk....",
				"......kk333399WWWW882222kk......",
				"........kk3333WWWW2222kk........",
				"..........kk33332222kk..........",
				"............kk3322kk............",
				"..............kkkk..............",
			},
		});

	SetArt(&arts, SPRITE_BLOCK_RUIN,
		{
			{
				"................................",
				"............l3..................",
				"............3333....3l..........",
				"........33..33ll....2l..........",
				"......ll3llll333l3l3..32........",
				"....l3l3333l233l32l3..l3..l3....",
				"..22l33l..223ll223l2ll233322....",
				"l33ll2l3..3l22lll2l3233l333l....",
				"..3l....l2ll....2l....3l..23ll2l",
				"..l3....lll3....l2....3l..222l..",
				"......3223l32l..2l..............",
				"..43..22l3l323..2l..............",
				"k433....lll3ll..l23l323l......2k",
				"k4334l..ll2223..2l3223......2l4k",
				"kl4l4l..l3..3322222l....ll..442k",
				"klll44l4333l..222l22....224l21lk",
				"kl34llll3333......21222444242l1k",
				"k43443334ll343..l1ll2244122422lk",
				"k4l43333ll444l..112144222224224k",
				"kk4l33333l4444..l24l2222l2ll4lkk",
				"..kk4l33l4343ll4442422l1l244kk..",
				"....kk4444l4343l22ll2222l4kk....",
				"......kk443433l3222l224lkk......",
				"........kk4ll333l22444kk........",
				"..........kk44l322llkk..........",
				"............kkl44lkk............",
				"..............kkkk..............",
			},
			{
				"................................",
				"............33....l3............",
				"............l33l22..l2..........",
				"........33..3l32l2..2l..........",
				"......2l322l3lll232l32l323......",
				"....l2333ll2232332l2333l2l2l....",
				"..33222333......23l32332l32223..",
				"..23223l2l......l3ll233332333332",
				"23................3323l2..32l232",
				"kl................l3333l..23l3..",
				"k3....l2ll2l....23l323232l....2k",
				"kl....222l33....23l3322233....lk",
				"k33l....l33l222323232222......lk",
				"k433ll34..3l2l33232ll324..l2l2lk",
				"k344l43ll3....l33323l224..l2441k",
				"k3l4l43l33....2l23..2lll..l42llk",
				"k334334433..........2224l424222k",
				"kll433l34l..........22l42224l22k",
				"kll4333llll4..4ll1..44l2ll2l21lk",
				"kk4l334344lll43l22442212222l44kk",
				"..kk443ll334ll44442l2l2l1244kk..",
				"....kkll33l4ll3l222422l244kk....",
				"......kk444l344l2ll42244kk......",
				"........kk443333l2l444kk........",
				"..........kk44l422l4kk..........",
				"............kk4l44kk............",
				"..............kkkk..............",
			},
		});

	SetArt(&arts, SPRITE_TENT,
		{
			{
				".............kCck.............",
				"............kCCcck............",
				"............kCCcck............",
				"...........kCCCccck...........",
				"..........kCCCCcccck..........",
				".........kCCCCCccccck.........",
				"........kCCCCCCcccccck........",
				"........kCCCCCCcccccck........",
				".......kCCCCCCkkcccccck.......",
				"......kCCCCCCCkkccccccck......",
				".....kCCCCCCCkTtkccccccck.....",
				".....kCCCCCCCkTtkccccccck.....",
				"....kCCCCCCCkTTttkccccccck....",
				"...kCCCCCCCCkTTttkcccccccck...",
				"..kCCCCCCCCkTTTtttkcccccccck..",
				".kCCCCCCCCCkTTTtttkccccccccck.",
				".kCCCCCCCCkTTTTttttkcccccccck.",
				"kCCCCCCCCCkTTTTttttkccccccccck",
				"CCCCCCCCCkTTTTTtttttkccccccccc",
				"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
			},
			{
				".............kBbk.............",
				"............kBBbbk............",
				"............kBBbbk............",
				"...........kBBBbbbk...........",
				"..........kBBBBbbbbk..........",
				".........kBBBBBbbbbbk.........",
				"........kBBBBBBbbbbbbk........",
				"........kBBBBBBbbbbbbk........",
				".......kBBBBBBkkbbbbbbk.......",
				"......kBBBBBBBkkbbbbbbbk......",
				".....kBBBBBBBkRrkbbbbbbbk.....",
				".....kBBBBBBBkRrkbbbbbbbk.....",
				"....kBBBBBBBkRRrrkbbbbbbbk....",
				"...kBBBBBBBBkRRrrkbbbbbbbbk...",
				"..kBBBBBBBBkRRRrrrkbbbbbbbbk..",
				".kBBBBBBBBBkRRRrrrkbbbbbbbbbk.",
				".kBBBBBBBBkRRRRrrrrkbbbbbbbbk.",
				"kBBBBBBBBBkRRRRrrrrkbbbbbbbbbk",
				"BBBBBBBBBkRRRRRrrrrrkbbbbbbbbb",
				"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
			},
		});

	SetArt(&arts, SPRITE_BANNER,
		{
			{
				".kk......",
				"kakkkkkkk",
				"kWkTttttk",
				"kWkToootk",
				"kWkToTotk",
				"kWkToTotk",
				"kWkTtttTk",
				"kWkTttTTk",
				"kWkTTkTTk",
				"kWkTk.kTk",
				"kWkk...kk",
				".kWk.....",
				".kWk.....",
				".kWk.....",
				".kWk.....",
				".kWk.....",
				".kWk.....",
				".kWk.....",
				".kWk.....",
				".kWk.....",
				".kWk.....",
				".kWk.....",
				".kWk.....",
				".kWk.....",
				"kWWWk....",
				"kkkkk....",
			},
		});

	SetArt(&arts, SPRITE_TOTEM,
		{
			{
				"..kkkk...",
				".kooook..",
				".kokoko..",
				".koooOk..",
				"..kOnOk..",
				"...kWk...",
				".rkrWkr..",
				"rRkrWkRr.",
				".r.kWk.r.",
				"...kWk...",
				"...kWk...",
				"...kWk...",
				"...kWk...",
				"...kWk...",
				"...kWk...",
				"...kWk...",
				"...kWk...",
				"...kWk...",
				"...kWk...",
				"...kWk...",
				".kkWWWkk.",
				"kBBBBBBBk",
			},
		});

	SetArt(&arts, SPRITE_GRAVE,
		{
			{
				"...k...",
				"..kwk..",
				"..kwk..",
				"kkkwkkk",
				"kwwwwwk",
				"kkkwkkk",
				"..kwt..",
				"..kwTt.",
				"..kwkT.",
				".kBBBk.",
				"kBBBBBk",
			},
		});

	SetArt(&arts, SPRITE_POUCH,
		{
			{
				"..kk..",
				".kbbk.",
				"kbyybk",
				"kbBbBk",
				".kkkk.",
			},
		});

	SetArt(&arts, SPRITE_HERB,
		{
			{
				"...h...",
				".h.g.h.",
				"..hgh..",
				".gGgGg.",
				"..gGg..",
				"...G...",
			},
		});

	SetArt(&arts, SPRITE_ARROW,
		{
			{
				".a.",
				"aAa",
				".W.",
				".W.",
				".W.",
				".W.",
				".W.",
				"cWc",
				"c.c",
			},
		});

	SetArt(&arts, SPRITE_STALL,
		{
			{
				"..kcccrrrcccrrrcccrrrcccrk..",
				".krcccrrrcccrrrcccrrrcccrrk.",
				".krcccrrrcccrrrcccrrrcccrrk.",
				"krrcccrrrcccrrrcccrrrcccrrrk",
				"krrcccrrrcccrrrcccrrrcccrrrk",
				"rrrcccrrrcccrrrcccrrrcccrrrc",
				"RRRCCCRRRCCCRRRCCCRRRCCCRRRC",
				"kkWkkkkkkkkkkkkkkkkkkkkkkWkk",
				".kWk....................kWk.",
				".kWk....................kWk.",
				".kWk....................kWk.",
				".kWk....................kWk.",
				".kWk.k.kkk.....kkkk.k.kkkWk.",
				".kWkkgkggBk.kkkBBBBkgkBBkWk.",
				".kWkygkggbkkyyybbbbkgybbkWk.",
				"kwwwwwwwwwwwwwwwwwwwwwwwwwwk",
				"kwwwwwwwwwwwwwwwwwwwwwwwwwwk",
				"kWWWWWWWWWWWWWWWWWWWWWWWWWWk",
				"kWWWWWWWWWWWWWWWWWWWWWWWWWWk",
				"kWWWWWWWWWWWWWWWWWWWWWWWWWWk",
				"kWWWWWWWWWWWWWWWWWWWWWWWWWWk",
				"kWWWWWWWWWWWWWWWWWWWWWWWWWWk",
				"kWWWWWWWWWWWWWWWWWWWWWWWWWWk",
				".kkkkkkkkkkkkkkkkkkkkkkkkkk.",
			},
		});

	SetArt(&arts, SPRITE_CROPS,
		{
			{
				".g...g...",
				"gGg.gGg.g",
				".G...G...",
			},
			{
				".D...0...",
				"0D0.D0D.0",
				".0...X...",
			},
		});

	SetArt(&arts, SPRITE_LAMP,
		{
			{
				".kkk.",
				"kHHHk",
				"kHIHk",
				".kdk.",
				".kdk.",
				".kdk.",
				".kdk.",
				".kdk.",
				".kdk.",
				".kdk.",
				".kdk.",
				".kdk.",
				".kdk.",
				".kdk.",
				".kdk.",
				".kdk.",
				".kdk.",
				".kddk",
				"kkkkk",
			},
		});

	return arts;
}
