// Généré par tools/facades/gen_layouts.py — ne pas modifier à la main

static PanelDef make_a2500()
{
    PanelDef p;
    p.model = "a2500"; p.width = 2172; p.height = 724;
    p.buttons.push_back ({ "knee", "knee", { 0.000f, 6.000f, 12.000f }, { { 1189, 199, 115, 58 }, { 1188, 280, 117, 60 }, { 1189, 361, 115, 58 } }, 14, false, {  } });
    p.buttons.push_back ({ "link", "link", { 0.000f, 50.000f, 100.000f }, { { 79, 565, 85, 59 }, { 175, 565, 84, 59 }, { 269, 565, 91, 61 } }, 14, false, {  } });
    p.buttons.push_back ({ "type", "feedback", { 0.000f, 1.000f }, { { 411, 565, 100, 62 }, { 522, 565, 94, 60 } }, 14, false, {  } });
    p.buttons.push_back ({ "punch", "punch", { 0.000f, 1.000f, 2.000f, 3.000f }, { { 932, 566, 78, 57 }, { 1021, 566, 78, 57 }, { 1110, 565, 82, 59 }, { 1203, 566, 77, 57 } }, 14, false, {  } });
    p.leds.push_back ({ "auto", "autorel", { 1072, 336, 74, 52 } });
    p.knobs.push_back ({ "threshold", "threshold", 219.500f, 312.500f, 77.000f, 163, { { -40.000f, -123.000f }, { -30.000f, -92.000f }, { -24.000f, -60.000f }, { -18.000f, -28.000f }, { -12.000f, 1.000f }, { -8.000f, 30.000f }, { -4.000f, 61.000f }, { -2.000f, 92.000f }, { 0.000f, 125.000f } }, false, 1.000f, 0.000f, NAN, 3.080f, 75.460f, 6.545f, 0xfff4f4f6, "%.1f dB" });
    p.knobs.push_back ({ "ratio", "ratio", 484.500f, 306.500f, 68.000f, 145, { { 1.500f, -129.000f }, { 3.000f, -52.000f }, { 4.000f, 0.000f }, { 6.000f, 51.000f }, { 10.000f, 129.000f } }, true, 1.000f, 0.000f, NAN, 2.720f, 66.640f, 5.780f, 0xfff4f4f6, "%.1f:1" });
    p.knobs.push_back ({ "attack", "attack", 743.500f, 310.500f, 71.000f, 151, { { 0.100f, -114.000f }, { 0.300f, -61.000f }, { 1.000f, -20.000f }, { 3.000f, 21.000f }, { 10.000f, 59.000f }, { 30.000f, 115.000f } }, true, 1.000f, 0.000f, NAN, 2.840f, 69.580f, 6.035f, 0xfff4f4f6, "%.2f ms" });
    p.knobs.push_back ({ "release", "release", 1010.500f, 312.500f, 71.000f, 151, { { 0.100f, -113.000f }, { 0.300f, -59.000f }, { 0.600f, -23.000f }, { 1.200f, 24.000f }, { 2.500f, 61.000f } }, true, 0.001f, 0.000f, 113.000f, 2.840f, 69.580f, 6.035f, 0xfff4f4f6, "%.2f s" });
    p.knobs.push_back ({ "hpf", "schpf", 773.500f, 610.500f, 40.000f, 89, { { 0.000f, -106.000f }, { 60.000f, -57.000f }, { 100.000f, 1.000f }, { 150.000f, 57.000f }, { 250.000f, 101.000f } }, false, 1.000f, 0.000f, NAN, 1.600f, 39.200f, 3.400f, 0xfff4f4f6, "%.0f Hz" });
    p.knobs.push_back ({ "mix", "mix", 1414.500f, 587.500f, 47.000f, 103, { { 0.000f, -137.000f }, { 100.000f, 137.000f } }, false, 1.000f, 0.000f, NAN, 1.880f, 46.060f, 3.995f, 0xfff4f4f6, "%.0f %%" });
    p.knobs.push_back ({ "output", "output", 1658.500f, 594.500f, 47.000f, 103, { { -20.000f, -125.000f }, { -10.000f, -73.000f }, { 0.000f, 0.000f }, { 10.000f, 74.000f }, { 20.000f, 125.000f } }, false, 1.000f, 0.000f, NAN, 1.880f, 46.060f, 3.995f, 0xfff4f4f6, "%+.1f dB" });
    p.needles.push_back ({ "grL", 0, 1548.000f, 500.000f, 228.000f, { 1397, 205, 295, 181 }, { { 20.000f, -25.800f }, { 0.000f, 25.800f } }, "gr" });
    p.needles.push_back ({ "grR", 1, 1910.000f, 500.000f, 228.000f, { 1759, 205, 295, 181 }, { { 20.000f, -25.800f }, { 0.000f, 25.800f } }, "gr" });
    return p;
}

static PanelDef make_fet76()
{
    PanelDef p;
    p.model = "fet76"; p.width = 2172; p.height = 724;
    p.buttons.push_back ({ "ratio", "ratio", { 4.000f, 8.000f, 12.000f, 20.000f }, { { 1186, 239, 103, 51 }, { 1186, 315, 103, 52 }, { 1186, 392, 103, 51 }, { 1187, 469, 101, 50 } }, 14, false, { { "fetall", 0.000f } } });
    p.buttons.push_back ({ "all", "fetall", { 1.000f }, { { 1186, 550, 103, 45 } }, 14, true, {  } });
    p.knobs.push_back ({ "input", "threshold", 268.500f, 399.500f, 89.000f, 187, { { 0.000f, -132.000f }, { 36.000f, 132.000f } }, false, -1.000f, 0.000f, NAN, 26.700f, 80.000f, 7.000f, 0xff1a1a1c, "%.0f" });
    p.knobs.push_back ({ "output", "output", 642.500f, 400.500f, 90.000f, 189, { { 0.000f, -132.000f }, { 36.000f, 132.000f } }, false, 1.000f, 18.000f, NAN, 27.000f, 80.000f, 7.000f, 0xff1a1a1c, "%.0f" });
    p.knobs.push_back ({ "attack", "attack", 964.500f, 306.500f, 43.000f, 95, { { 0.800f, -135.000f }, { 0.020f, 135.000f } }, true, 1.000f, 0.000f, NAN, 12.900f, 40.000f, 4.000f, 0xff1a1a1c, "%.3f ms" });
    p.knobs.push_back ({ "release", "release", 965.500f, 527.500f, 44.000f, 97, { { 1100.000f, -135.000f }, { 50.000f, 135.000f } }, true, 1.000f, 0.000f, NAN, 13.200f, 41.000f, 4.000f, 0xff1a1a1c, "%.0f ms" });
    p.knobs.push_back ({ "mix", "mix", 2012.500f, 392.500f, 43.000f, 95, { { 0.000f, -140.000f }, { 100.000f, 140.000f } }, false, 1.000f, 0.000f, NAN, 12.900f, 40.000f, 4.000f, 0xff1a1a1c, "%.0f %%" });
    p.needles.push_back ({ "grL", 0, 1648.000f, 670.000f, 350.000f, { 1430, 238, 428, 242 }, { { 20.000f, -29.000f }, { 10.000f, -19.500f }, { 7.000f, -12.300f }, { 5.000f, -5.000f }, { 3.000f, 4.900f }, { 2.000f, 12.300f }, { 1.000f, 19.300f }, { 0.000f, 27.800f } }, "gr" });
    return p;
}

static const PanelDef* findPanel (const juce::String& model)
{
    static const std::vector<PanelDef> all { make_a2500(), make_fet76() };
    for (auto& p : all) if (model == p.model) return &p;
    return nullptr;
}
