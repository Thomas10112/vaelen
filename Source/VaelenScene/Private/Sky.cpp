// VAELEN - VaelenScene
// Phase 19 task 19.09: the cold and the heat can be seen. See Sky.h.
//
// STATUS: VALIDATED headless (Phase 19 task 19.09)
#include "Vaelen/Scene/Sky.h"

#include "Vaelen/Scene/LineWriter.h"

namespace Vaelen::Scene
{
	namespace
	{
		/// sin of whole degrees 0..90, times 10000: the only trigonometry here.
		constexpr int32 Sine[91] = {0,	  175,	349,  523,	698,  872,	1045, 1219, 1392, 1564, 1736, 1908, 2079,
									2250, 2419, 2588, 2756, 2924, 3090, 3256, 3420, 3584, 3746, 3907, 4067, 4226,
									4384, 4540, 4695, 4848, 5000, 5150, 5299, 5446, 5592, 5736, 5878, 6018, 6157,
									6293, 6428, 6561, 6691, 6820, 6947, 7071, 7193, 7314, 7431, 7547, 7660, 7771,
									7880, 7986, 8090, 8192, 8290, 8387, 8480, 8572, 8660, 8746, 8829, 8910, 8988,
									9063, 9135, 9205, 9272, 9336, 9397, 9455, 9511, 9563, 9613, 9659, 9703, 9744,
									9781, 9816, 9848, 9877, 9903, 9925, 9945, 9962, 9976, 9986, 9994, 9998, 10000};

		/// sin of any whole degree, times 10000.
		int32 SinDeg(int32 Degrees)
		{
			int32 D = Degrees % 360;
			D = D < 0 ? D + 360 : D;
			if (D <= 90)
			{
				return Sine[D];
			}
			if (D <= 180)
			{
				return Sine[180 - D];
			}
			if (D <= 270)
			{
				return -Sine[D - 180];
			}
			return -Sine[360 - D];
		}

		constexpr uint8 SnowR = 236, SnowG = 240, SnowB = 246;
	} // namespace

	uint8 SnowOf(const View::TileClimate& T)
	{
		if ((T.Flags & View::ClimateFlag::Frost) == 0u)
		{
			return 0u;
		}
		if (T.Now <= FullSnowDegrees)
		{
			return 255u;
		}
		// -4 .. 0 (and a frost flag above a floor of 0, from a fraction below
		// the line): a thin cover, never none while it freezes.
		const int32 Depth = T.Now < 0 ? -int32{T.Now} : 0;
		return static_cast<uint8>(51 + Depth * 204 / (-FullSnowDegrees));
	}

	uint8 GrassOf(const View::TileClimate& T)
	{
		return (T.Flags & View::ClimateFlag::Growing) != 0u && (T.Flags & View::ClimateFlag::Frost) == 0u ? 24u : 0u;
	}

	void ApplyClimate(const Ground& G, const View::ClimateView& Climate, TerrainMesh& Mesh)
	{
		if (Climate.Tiles.empty() || Climate.Width != G.Width || Climate.Height != G.Height)
		{
			return;
		}
		const int64 L = G.Scale.CmPerTile / G.Scale.Steps;
		for (TerrainVertex& V : Mesh.Vertices)
		{
			// The tile the vertex took its colour from (Terrain.cpp's rule).
			const int64 U = V.X / L + G.Scale.Steps / 2;
			const int64 W = V.Y / L + G.Scale.Steps / 2;
			int64 TX = U / G.Scale.Steps;
			int64 TY = W / G.Scale.Steps;
			TX = TX < 0 ? 0 : (TX >= G.Width ? G.Width - 1 : TX);
			TY = TY < 0 ? 0 : (TY >= G.Height ? G.Height - 1 : TY);
			const uint32 Tile = static_cast<uint32>(TY) * G.Width + static_cast<uint32>(TX);
			if (G.Kind[Tile] == GroundKind::Sea)
			{
				continue;
			}
			const View::TileClimate& T = Climate.Tiles[Tile];
			const uint32 Snow = SnowOf(T);
			if (Snow != 0u)
			{
				V.R = static_cast<uint8>((V.R * (255u - Snow) + SnowR * Snow) / 255u);
				V.G = static_cast<uint8>((V.G * (255u - Snow) + SnowG * Snow) / 255u);
				V.B = static_cast<uint8>((V.B * (255u - Snow) + SnowB * Snow) / 255u);
			}
			else if (G.Kind[Tile] == GroundKind::Land)
			{
				const uint32 Green = V.G + GrassOf(T);
				V.G = static_cast<uint8>(Green > 255u ? 255u : Green);
			}
		}
	}

	Sun SunOf(uint32 Row, uint32 Height, uint32 DayOfYear, uint32 Spent, uint32 Awake)
	{
		Sun Out;
		if (Height == 0u)
		{
			return Out;
		}
		// Distance to the equator row, whole degrees 0..90.
		const int32 Mid = static_cast<int32>(Height - 1u);
		const int32 Twice = 2 * static_cast<int32>(Row) - Mid;
		const int32 Latitude = (Twice < 0 ? -Twice : Twice) * 90 / (Mid == 0 ? 1 : Mid);
		// The declination, 23.4 deg at mid-summer (day 135) and -23.4 at day 315.
		const int32 Declination = 234 * SinDeg(static_cast<int32>(DayOfYear) - 45) / 10000; // tenths
		// Noon's height is 90 deg less the angle between the row and the sun's
		// declination; on a row nearer the equator than the declination the
		// sun passes the zenith's other side, lower again.
		const int32 Off = Latitude * 10 - Declination;
		const int32 Noon = 900 - (Off < 0 ? -Off : Off); // tenths
		if (Awake == 0u)
		{
			Out.Azimuth = 1800;
			return Out;
		}
		const uint32 Into = Spent > Awake ? Awake : Spent;
		// East at the day's first hour, south at its middle, west at its last;
		// the height rises and falls as sin of the day's fraction.
		const int32 Arc = static_cast<int32>(Into * 180u / Awake); // degrees 0..180
		Out.Azimuth = 900 + static_cast<int32>(Into * 1800u / Awake);
		Out.Elevation = Noon * SinDeg(Arc) / 10000;
		return Out;
	}

	BodyWeather BodyOf(const View::LifeView& Life)
	{
		BodyWeather Out;
		if (Life.Person == 0u || Life.Alive == 0u)
		{
			return Out;
		}
		Out.Breath = static_cast<uint8>(Life.Degrees < 0 ? 1u : 0u);
		Out.Shiver = static_cast<uint8>(Life.Chill > 255u ? 255u : Life.Chill);
		return Out;
	}

	SkyStats MeasureSky(const Ground& G, const View::ClimateView& Climate, const View::LifeView& Life,
						uint32 CentroidRow)
	{
		SkyStats S;
		Hash64 H = HashCombine(HashUInt64(Climate.Day), HashUInt64(Climate.Tiles.size()));
		for (usize i = 0; i < Climate.Tiles.size(); ++i)
		{
			const bool Sea = i < G.Kind.size() && G.Kind[i] == GroundKind::Sea;
			const uint8 Snow = Sea ? 0u : SnowOf(Climate.Tiles[i]);
			const uint8 Grass = Sea ? 0u : GrassOf(Climate.Tiles[i]);
			S.Snow += Snow != 0u ? 1u : 0u;
			S.Growing += Grass != 0u ? 1u : 0u;
			H = HashCombine(H, HashUInt64((uint64{Snow} << 8) | Grass));
		}
		S.Tiles = static_cast<uint32>(Climate.Tiles.size());
		const Sun Now = SunOf(CentroidRow, G.Height, Climate.Day, Life.Spent, Life.Awake);
		const BodyWeather Body = BodyOf(Life);
		S.Azimuth = Now.Azimuth;
		S.Elevation = Now.Elevation;
		S.Breath = Body.Breath;
		S.Shiver = Body.Shiver;
		H = HashCombine(H, HashUInt64((static_cast<uint64>(static_cast<uint32>(Now.Azimuth)) << 32) |
									  static_cast<uint32>(Now.Elevation)));
		H = HashCombine(H, HashUInt64((uint64{Body.Breath} << 8) | Body.Shiver));
		S.Digest = H;
		return S;
	}

	uint32 SkyLine(uint32 Size, uint64 Seed, uint32 Day, const SkyStats& S, char* Out, uint32 Bytes)
	{
		if (Out == nullptr || Bytes == 0u)
		{
			return 0u;
		}
		Detail::LineWriter W{Out, Bytes};
		W.Put("LogVaelenScene: AELVOR ");
		W.Unsigned(Size);
		W.Put(" seed ");
		W.Hex(Seed, 12u);
		W.Put(" sky day ");
		W.Unsigned(Day);
		W.Put(": snow ");
		W.Unsigned(S.Snow);
		W.Put(" of ");
		W.Unsigned(S.Tiles);
		W.Put(" tiles, growing ");
		W.Unsigned(S.Growing);
		W.Put(", sun azimuth ");
		W.Signed(S.Azimuth);
		W.Put(" elevation ");
		W.Signed(S.Elevation);
		W.Put(", breath ");
		W.Unsigned(S.Breath);
		W.Put(" shiver ");
		W.Unsigned(S.Shiver);
		W.Put("; sky ");
		W.Hex(S.Digest, 16u);
		return W.Finish();
	}
} // namespace Vaelen::Scene
