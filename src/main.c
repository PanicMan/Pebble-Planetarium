#include <pebble.h>
#include "utils.h"
	
#define TIMER_MS 150

enum ConfigKeys {
	CONFIG_KEY_INV=1,
	CONFIG_KEY_ANIM=2,
	CONFIG_KEY_STARS=3,
	CONFIG_KEY_VIBR=4,
	CONFIG_KEY_DATE=5,
	CONFIG_KEY_ASTRO=6,
	CONFIG_KEY_INFR=7
};

typedef struct {
	bool inv;
	bool anim;
	bool stars;
	bool vibr;
	bool astro;
	bool infr;
	char date[9];
} CfgDta_t;

typedef struct {	//J2000 Planet Data
	char name[8];	//Name
	int16_t radius;	//Radius
	int16_t size;	//Size
	double M;		//Mean Anomaly (deg)
	double Md;		//Vertical Mean Anomaly (deg)
	double e;		//Orbital Eccentricity
	double ed;		//Vertical Orbital Eccentricity
	int32_t angleC; //Angle, Calculated on the fly
	int32_t sinC;	//Sinus, Calculated on the fly
	int32_t cosC;	//Cosin, Calculated on the fly
} Planet_t;

#define PLANETS_MAX 8
static Planet_t Planets[] = { 
	{"Merkur", 20, 3, 252.25084, 4.0923344368, 0.205635, 5.59E-10, 0, 0, 0},
	{"Venus", 30, 4, 181.97973, 1.602130474, 0.006773, -1.302E-9, 0, 0, 0},
	{"Earth", 50, 6, 100.46435, 0.985609101, 0.016709, -1.151E-9, 0, 0, 0},
	{"Mars", 70, 5, 355.45332, 0.524033035, 0.093405, 2.516E-9, 0, 0, 0},
	{"Jupiter", 85, 7, 34.40438, 0.0830853001, 0.048498, 4.469E-9, 0, 0, 0},
	{"Saturn", 100, 6, 49.94432, 0.033470629, 0.055546, -9.499E-9, 0, 0, 0},
	{"Uranus", 115, 4, 313.23218, 0.011731294, 0.047318, 7.45E-9, 0, 0, 0},
	{"Neptun", 130, 4, 304.88003, 0.0059810572, 0.008606, 2.15E-9, 0, 0, 0}
};

static Planet_t Moon = {"Moon", 10, 2, 64.975464, 13.0649929509, 0.054900, 0.0, 0, 0, 0};
static Planet_t Star = {"Star", 50, 1, 100.46435, 0.985609101, 0.016709, -1.151E-9, 0, 0, 0};

#define STARS_MAX 50
static GPoint Stars[STARS_MAX];

#define ASTRO_MAX 400
static GPoint Astro[ASTRO_MAX];

static const struct GPathInfo HAND_PATH_INFO = {
 	//.num_points = 7, 
	//.points = (GPoint[]) {{-6, 0}, {-3, 6}, {-3, 20}, {0, 23}, {3, 20}, {3, 6}, {6, 0}}
 	//.num_points = 3, 
	//.points = (GPoint[]) {{-6, 0}, {0, 23}, {6, 0}}
 	.num_points = 18, 
	.points = (GPoint[]) {
		{-2, -4}, {-4, 0}, {-6, -4}, {0, 23}, 
		{-4, 21}, {-3, 25}, {-6, 27}, {-2, 28}, {0, 32}, {2, 28}, {6, 27}, {3, 25}, {4, 21}, {0, 23},
		{6, -4}, {4, 0}, {2, -4}, {0, 0}}
};

static const struct GPathInfo STAR_PATH_INFO = {
 	.num_points = 10, 
	.points = (GPoint[]) {{-5,-7}, {0,-16}, {5,-7}, {15,-4}, {8,3}, {10,14}, {0,8}, {-10,14}, {-8,3}, {-15,-4}}
};

GPath *hand_path, *star_path;

static const uint32_t segments[] = {100, 100, 100};
static const VibePattern vibe_pat = {
	.durations = segments,
	.num_segments = ARRAY_LENGTH(segments),
};

Window *window;
Layer *face_layer;

static GFont digitS;
char hhBuffer[] = "00";
static int16_t aktHH, aktMM;
static AppTimer *timer;
static bool b_initialized;
static CfgDta_t CfgData;

//-----------------------------------------------------------------------------------------------------------------------
static void face_update_proc(Layer *layer, GContext *ctx) 
{
	GColor cNormal, cInverted;
	GRect bounds = layer_get_bounds(layer);
	GPoint /*center = grect_center_point(&bounds),*/ clock_center = GPoint(200, 200);

#ifdef PBL_COLOR
	cNormal = GColorRajah;
	cInverted = GColorWindsorTan;
#else
	cNormal = GColorWhite;
	cInverted = GColorBlack;
#endif

	graphics_context_set_stroke_color(ctx, CfgData.inv ? cInverted : cNormal);
	graphics_context_set_fill_color(ctx, CfgData.inv ? cInverted : cNormal);
	graphics_context_set_text_color(ctx, CfgData.inv ? cInverted : cNormal);
	
	//TRIG_MAX_ANGLE * t->tm_sec / 60
	int32_t angle = (TRIG_MAX_ANGLE * (((aktHH % 12) * 60) + (aktMM / 1))) / (12 * 60),	sinl = sin_lookup(angle), cosl = cos_lookup(angle);
	int16_t radV = 85, radD = 145, radT = 145;
	
	GPoint sub_center, ptLin, ptDot;
	sub_center.x = (int16_t)(sinl * (int32_t)radV / TRIG_MAX_RATIO) + clock_center.x;
	sub_center.y = (int16_t)(-cosl * (int32_t)radV / TRIG_MAX_RATIO) + clock_center.y;

	GRect sub_rect = {
		.origin = GPoint(sub_center.x - bounds.size.w / 2, sub_center.y - bounds.size.h / 2),
		.size = bounds.size
	};

	//Draw Points and Hours
	for (int32_t i = 1; i<=48; i++)
	{
		int32_t angleC = TRIG_MAX_ANGLE * i / 48,
			sinC = sin_lookup(angleC), cosC = cos_lookup(angleC);
		
		ptLin.x = (int16_t)(sinC * (int32_t)(radD) / TRIG_MAX_RATIO) + clock_center.x - sub_rect.origin.x;
		ptLin.y = (int16_t)(-cosC * (int32_t)(radD) / TRIG_MAX_RATIO) + clock_center.y - sub_rect.origin.y;

		if (ptLin.x > -10 && ptLin.x < bounds.size.w+10 && ptLin.y > -10 && ptLin.y < bounds.size.h+10)
		{
			if ((i % 4) == 0)
			{
				snprintf(hhBuffer, sizeof(hhBuffer), "%d", (int16_t)i/4);
				GSize txtSize = graphics_text_layout_get_content_size(hhBuffer, digitS, 
					bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter);

				ptDot.x = (int16_t)(sinC * (int32_t)radT / TRIG_MAX_RATIO) + clock_center.x - sub_rect.origin.x;
				ptDot.y = (int16_t)(-cosC * (int32_t)radT / TRIG_MAX_RATIO) + clock_center.y - sub_rect.origin.y;

				graphics_draw_text(ctx, hhBuffer, digitS, 
					GRect(ptDot.x-txtSize.w/2, ptDot.y-txtSize.h/2-3, txtSize.w, txtSize.h), 
					GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
			}
			else
			{
				graphics_fill_circle(ctx, ptLin, 2);
			}
		}
	}
	
	//Draw Stars
	if (CfgData.stars)
		for (int i=0; i<STARS_MAX; i++)
		{
			GPoint ptStar = {
				.x = Stars[i].x + clock_center.x - sub_rect.origin.x, 
				.y = Stars[i].y + clock_center.y - sub_rect.origin.y
			};

			if (grect_contains_point(&bounds, &ptStar))
			{
				#ifdef PBL_COLOR
					int8_t rnd = rand() % 4;
					graphics_context_set_stroke_color(ctx, rnd == 0 ? GColorBlack : rnd == 1 ? GColorDarkGray : rnd == 2 ? GColorLightGray : GColorWhite);
				#endif

				graphics_draw_pixel(ctx, ptStar);
			}
		}
	
	graphics_context_set_stroke_color(ctx, CfgData.inv ? GColorBlack : GColorWhite);
	
	//Draw Asteorids
	if (CfgData.astro)
		for (int i=0; i<ASTRO_MAX; i++)
		{
			GPoint ptAstro = {
				.x = Astro[i].x + clock_center.x - sub_rect.origin.x, 
				.y = Astro[i].y + clock_center.y - sub_rect.origin.y
			};
			if (grect_contains_point(&bounds, &ptAstro))
				graphics_draw_pixel(ctx, ptAstro);
		}
	
	//Draw Sun
	ptLin.x = clock_center.x - sub_rect.origin.x;
	ptLin.y = clock_center.y - sub_rect.origin.y;
	#ifdef PBL_COLOR
		graphics_context_set_fill_color(ctx, GColorYellow);
		graphics_fill_circle(ctx, ptLin, 15);
		graphics_context_set_fill_color(ctx, GColorIcterine);
		graphics_fill_circle(ctx, ptLin, 10);
		graphics_context_set_fill_color(ctx, GColorPastelYellow);
		graphics_fill_circle(ctx, ptLin, 5);
	#else
		graphics_fill_circle(ctx, ptLin, 15);
	#endif	

	#ifdef PBL_COLOR
		graphics_context_set_stroke_color(ctx, CfgData.inv ? GColorLightGray : GColorDukeBlue);
	#endif
	//Draw Planet Orbits
	for (int i=0; i<PLANETS_MAX; i++)
		graphics_draw_circle(ctx, ptLin, Planets[i].radius);
	
	//Draw Planets
	for (int i=0; i<PLANETS_MAX; i++)
	{
		ptLin.x = (int16_t)(Planets[i].sinC * (int32_t)Planets[i].radius / TRIG_MAX_RATIO) + clock_center.x - sub_rect.origin.x;
		ptLin.y = (int16_t)(-Planets[i].cosC * (int32_t)Planets[i].radius / TRIG_MAX_RATIO) + clock_center.y - sub_rect.origin.y;
		if (ptLin.x > -Planets[i].radius && ptLin.x < bounds.size.w+Planets[i].radius && ptLin.y > -Planets[i].radius && ptLin.y < bounds.size.h+Planets[i].radius)
		{
			#ifdef PBL_COLOR
				GColor cF = GColorWhite, cB = GColorBlack;
				switch (i)
				{
				case 0: //Merkur
					cF = GColorWindsorTan; cB = GColorBulgarianRose;
					break;
				case 1: //Venus
					cF = GColorPastelYellow; cB = GColorArmyGreen;
					break;
				case 2: //Earth
					cF = GColorVeryLightBlue; cB = GColorDukeBlue;
					break;
				case 3: //Mars
					cF = GColorRed; cB = GColorBulgarianRose;
					break;
				case 4: //Jupiter
					cF = GColorChromeYellow; cB = GColorBulgarianRose;
					break;
				case 5: //Saturn
					cF = GColorBrass; cB = GColorArmyGreen;
					break;
				case 6: //Uranus
					cF = GColorLiberty; cB = GColorDukeBlue;
					break;
				case 7: //Neptun
					cF = GColorElectricUltramarine; cB = GColorDukeBlue;
					break;
				}

				//Front side
				graphics_context_set_stroke_color(ctx, cB);
				DrawArc2(ctx, ptLin, Planets[i].size, Planets[i].size, Planets[i].angleC-85, Planets[i].angleC+85);
				//Back side
				graphics_context_set_stroke_color(ctx, cF);
				DrawArc2(ctx, ptLin, Planets[i].size, Planets[i].size, Planets[i].angleC+85, Planets[i].angleC+275);
			#else
				graphics_fill_circle(ctx, ptLin, Planets[i].size);
			#endif
			
			if (i == 2) //Moon at Earth
			{
				//Orbit
				#ifdef PBL_COLOR
					graphics_context_set_stroke_color(ctx, CfgData.inv ? GColorLightGray : GColorDukeBlue);
					graphics_draw_circle(ctx, ptLin, Moon.radius);
				#else
					graphics_draw_circle(ctx, ptLin, Moon.radius);
				#endif
				
				ptLin.x = (int16_t)(Moon.sinC * (int32_t)Moon.radius / TRIG_MAX_RATIO) + ptLin.x;
				ptLin.y = (int16_t)(-Moon.cosC * (int32_t)Moon.radius / TRIG_MAX_RATIO) + ptLin.y;
				#ifdef PBL_COLOR
					cF = GColorLightGray; cB = GColorDarkGray;
					//Front side
					graphics_context_set_stroke_color(ctx, cB);
					DrawArc2(ctx, ptLin, Moon.size, Moon.size, Planets[i].angleC-85, Planets[i].angleC+85);
					//Back side
					graphics_context_set_stroke_color(ctx, cF);
					DrawArc2(ctx, ptLin, Moon.size, Moon.size, Planets[i].angleC+85, Planets[i].angleC+275);
				#else
					graphics_fill_circle(ctx, ptLin, Moon.size);
				#endif
			}
			else if (i == 5) //Saturn rings
			{
				graphics_context_set_stroke_color(ctx, CfgData.inv ? GColorWhite : GColorBlack);
				DrawEllipse(ctx, ptLin.x, ptLin.y, 10, 3, 60, 300);
				graphics_context_set_stroke_color(ctx, CfgData.inv ? GColorBlack : GColorWhite);
				DrawEllipse(ctx, ptLin.x, ptLin.y, 11, 4, 60, 300);
				graphics_context_set_stroke_color(ctx, CfgData.inv ? GColorWhite : GColorBlack);
				DrawEllipse(ctx, ptLin.x, ptLin.y, 12, 5, 60, 300);
				graphics_context_set_stroke_color(ctx, CfgData.inv ? GColorBlack : GColorWhite);
			}
		}
	}

	//Draw Lucky Star
	if (Star.size != 0)
	{
		ptLin.x = (int16_t)(Star.sinC * (int32_t)Star.radius / TRIG_MAX_RATIO) + clock_center.x - sub_rect.origin.x;
		ptLin.y = (int16_t)(-Star.cosC * (int32_t)Star.radius / TRIG_MAX_RATIO) + clock_center.y - sub_rect.origin.y;
		if (ptLin.x > -10 && ptLin.x < bounds.size.w+10 && ptLin.y > -10 && ptLin.y < bounds.size.h+10)
		{
			gpath_move_to(star_path, ptLin);
			gpath_rotate_to(star_path, Star.angleC);
			graphics_context_set_stroke_color(ctx, CfgData.inv ? GColorBlack : GColorWhite);
			gpath_draw_outline(ctx, star_path);
		}
	}
	
	//Draw Hand Path, only if no infinite rotation
	if (!CfgData.infr)
	{
		ptLin.x = (int16_t)(sinl * (int32_t)(radD+26) / TRIG_MAX_RATIO) + clock_center.x - sub_rect.origin.x;
		ptLin.y = (int16_t)(-cosl * (int32_t)(radD+26) / TRIG_MAX_RATIO) + clock_center.y - sub_rect.origin.y;

		gpath_move_to(hand_path, ptLin);
		gpath_rotate_to(hand_path, angle);

		#ifdef PBL_COLOR
			graphics_context_set_fill_color(ctx, CfgData.inv ? GColorWindsorTan : GColorYellow);
		#else
			graphics_context_set_fill_color(ctx, CfgData.inv ? cInverted : cNormal);
		#endif
		gpath_draw_filled(ctx, hand_path);
		graphics_context_set_stroke_color(ctx, CfgData.inv ? GColorWhite : GColorBlack);
		gpath_draw_outline(ctx, hand_path);
	}
}
//-----------------------------------------------------------------------------------------------------------------------
static void handle_tick(struct tm *tick_time, TimeUnits units_changed) 
{
	//Calculate on Init or every Hour
	if (units_changed == YEAR_UNIT || tick_time->tm_min == 0)
	{
		int32_t d = FNday((tick_time->tm_year+1900), (tick_time->tm_mon+1), tick_time->tm_mday, tick_time->tm_hour), angleC;
	
		for (int i=0; i<PLANETS_MAX; i++)
		{
			Planets[i].angleC = 360-rev(Planets[i].M + (Planets[i].Md * (double)d));
			angleC = TRIG_MAX_ANGLE * Planets[i].angleC / 360; 
			Planets[i].sinC = sin_lookup(angleC); 
			Planets[i].cosC = cos_lookup(angleC);
			
			app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__,
            	"Planer %s: Angle %d", Planets[i].name, (int)Planets[i].angleC);
		}
		
		Moon.angleC = 360-rev(Moon.M + (Moon.Md * (double)d));
		angleC = TRIG_MAX_ANGLE * Moon.angleC / 360; 
		Moon.sinC = sin_lookup(angleC); 
		Moon.cosC = cos_lookup(angleC);

		if (Star.size != 0)
		{
			int16_t 
				year = (CfgData.date[0]-48)*1000+(CfgData.date[1]-48)*100+(CfgData.date[2]-48)*10+(CfgData.date[3]-48),
				month = (CfgData.date[4]-48)*10+(CfgData.date[5]-48),
				day = (CfgData.date[6]-48)*10+(CfgData.date[7]-48);
			
			app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__,
            	"Star Date (%d): %d.%d.%d", (int)Star.size, (int)day, (int)month, (int)year);
			
			d = FNday(((year ? year : tick_time->tm_year)+1900), ((month ? month : tick_time->tm_mon)+1), (day ? day : tick_time->tm_mday), 12);
			Star.angleC = 360-rev(Star.M + (Star.Md * (double)d));
			angleC = TRIG_MAX_ANGLE * Star.angleC / 360; 
			Star.sinC = sin_lookup(angleC); 
			Star.cosC = cos_lookup(angleC);
		}
	}
	
	//Hourly vibrate
	if (CfgData.vibr && tick_time->tm_min == 0)
		vibes_enqueue_custom_pattern(vibe_pat); 	

	if (b_initialized || units_changed == YEAR_UNIT)
	{
		aktHH = tick_time->tm_hour;
		aktMM = tick_time->tm_min;
		layer_mark_dirty(face_layer);
	}
}
//-----------------------------------------------------------------------------------------------------------------------
static void timerCallback(void *data) 
{
	if (!b_initialized)
	{
		time_t temp = time(NULL);
		struct tm *t = localtime(&temp);
		if ((aktHH % 12) != (t->tm_hour % 12) || aktMM != t->tm_min || CfgData.infr)
		{
			timer = app_timer_register(TIMER_MS, timerCallback, NULL);
			layer_mark_dirty(face_layer);
		
			int16_t nStep = CfgData.infr ? 5 : (aktHH % 12) != (t->tm_hour % 12) ? ((t->tm_hour % 12) < 6 ? 5 : 10) : 1;
			if (aktMM < 60-nStep)
				aktMM += nStep;
			else
			{
				aktMM = 0;
				aktHH++;
				aktHH = aktHH % 24; //Immer 0-23h
			}
		}
		else
			b_initialized = true;
	}
}
//-----------------------------------------------------------------------------------------------------------------------
static void update_configuration(void)
{
    if (persist_exists(CONFIG_KEY_INV))
		CfgData.inv = persist_read_bool(CONFIG_KEY_INV);
	else	
		CfgData.inv = false;
	
    if (persist_exists(CONFIG_KEY_ANIM))
		CfgData.anim = persist_read_bool(CONFIG_KEY_ANIM);
	else	
		CfgData.anim = true;
	
    if (persist_exists(CONFIG_KEY_STARS))
		CfgData.stars = persist_read_bool(CONFIG_KEY_STARS);
	else	
		CfgData.stars = true;
	
    if (persist_exists(CONFIG_KEY_VIBR))
		CfgData.vibr = persist_read_bool(CONFIG_KEY_VIBR);
	else	
		CfgData.vibr = false;
	
    if (persist_exists(CONFIG_KEY_ASTRO))
		CfgData.astro = persist_read_bool(CONFIG_KEY_ASTRO);
	else	
		CfgData.astro = false;
	
    if (persist_exists(CONFIG_KEY_INFR))
		CfgData.infr = persist_read_bool(CONFIG_KEY_INFR);
	else	
		CfgData.infr = false;
	
    if (persist_exists(CONFIG_KEY_DATE))
		persist_read_string(CONFIG_KEY_DATE, CfgData.date, sizeof(CfgData.date));
	else	
		strcpy(CfgData.date, "00000000");
	
	Star.size = atoi(CfgData.date) != 0 ? 1 : 0;
	
	app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "Curr Conf: inv:%d, anim:%d, stars:%d, vibr:%d, date:%s, astro:%d",
		CfgData.inv, CfgData.anim, CfgData.stars, CfgData.vibr, CfgData.date, CfgData.astro);
	
	//Layer *window_layer = window_get_root_layer(window);
	#ifdef PBL_COLOR
		window_set_background_color(window, CfgData.inv ? GColorBabyBlueEyes : GColorOxfordBlue);
	#else
		window_set_background_color(window, CfgData.inv ? GColorWhite : GColorBlack);
	#endif

	//Get a time structure so that it doesn't start blank
	time_t temp = time(NULL);
	struct tm *t = localtime(&temp);

	//Manually call the tick handler when the window is loading
	handle_tick(t, YEAR_UNIT);
	
	//If infinite rotation, start again
	if (CfgData.infr)
		b_initialized = false;
		
	//Start|Skip Animation
	if (CfgData.anim && !b_initialized)
	{
		aktHH = aktMM = 0;
		timerCallback(NULL);
	}	
	else
		b_initialized = true;
}
//-----------------------------------------------------------------------------------------------------------------------
void in_received_handler(DictionaryIterator *received, void *ctx)
{
	app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "enter in_received_handler");
    
	Tuple *akt_tuple = dict_read_first(received);
    while (akt_tuple)
    {
        app_log(APP_LOG_LEVEL_DEBUG,
                __FILE__,
                __LINE__,
                "KEY %d=%s", (int16_t)akt_tuple->key,
                akt_tuple->value->cstring);

		if (akt_tuple->key == CONFIG_KEY_INV)
			persist_write_bool(CONFIG_KEY_INV, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		if (akt_tuple->key == CONFIG_KEY_ANIM)
			persist_write_bool(CONFIG_KEY_ANIM, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		if (akt_tuple->key == CONFIG_KEY_STARS)
			persist_write_bool(CONFIG_KEY_STARS, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		if (akt_tuple->key == CONFIG_KEY_VIBR)
			persist_write_bool(CONFIG_KEY_VIBR, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		if (akt_tuple->key == CONFIG_KEY_ASTRO)
			persist_write_bool(CONFIG_KEY_ASTRO, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		if (akt_tuple->key == CONFIG_KEY_INFR)
			persist_write_bool(CONFIG_KEY_INFR, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		if (akt_tuple->key == CONFIG_KEY_DATE)
			persist_write_string(CONFIG_KEY_DATE, akt_tuple->value->cstring);
		
		akt_tuple = dict_read_next(received);
	}
	
    update_configuration();
}
//-----------------------------------------------------------------------------------------------------------------------
void in_dropped_handler(AppMessageResult reason, void *ctx)
{
    app_log(APP_LOG_LEVEL_WARNING,
            __FILE__,
            __LINE__,
            "Message dropped, reason code %d",
            reason);
}
//-----------------------------------------------------------------------------------------------------------------------
static void window_load(Window *window) 
{
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	
	digitS = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_25));
	
	// Init layers
	face_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
	layer_set_update_proc(face_layer, face_update_proc);
	layer_add_child(window_layer, face_layer);
	
	//Update Configuration
	update_configuration();
}
//-----------------------------------------------------------------------------------------------------------------------
static void window_unload(Window *window) 
{
	layer_destroy(face_layer);
	fonts_unload_custom_font(digitS);
	if (!b_initialized)
		app_timer_cancel(timer);
}
//-----------------------------------------------------------------------------------------------------------------------
static void init(void) 
{
	b_initialized = false;

	window = window_create();
	window_set_background_color(window, GColorBlack);
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});

	//Init Stars
	srand(time(NULL));
	for (int i=0; i<STARS_MAX; i++)
	{
		int32_t angleC = TRIG_MAX_ANGLE * i / STARS_MAX, sinC = sin_lookup(angleC), cosC = cos_lookup(angleC),
			rnd = rand() % 130;
		
		Stars[i].x = (int16_t)(sinC * (int32_t)(rnd+20) / TRIG_MAX_RATIO);
		Stars[i].y = (int16_t)(-cosC * (int32_t)(rnd+20) / TRIG_MAX_RATIO);
	}
	
	//Init Astros
	int16_t radius = (Planets[3].radius+Planets[4].radius)/2+1;
	for (int i=0; i<ASTRO_MAX; i++)
	{
		int32_t angleC = TRIG_MAX_ANGLE * i / ASTRO_MAX, sinC = sin_lookup(angleC), cosC = cos_lookup(angleC);
		int16_t	rnd = rand() % 100, delta = rnd < 10 ? -4 : rnd < 30 ? -2 : rnd > 90 ? 4 : rnd > 70 ? 2 : 0;
		
		Astro[i].x = (int16_t)(sinC * (int32_t)(radius+delta) / TRIG_MAX_RATIO);
		Astro[i].y = (int16_t)(-cosC * (int32_t)(radius+delta) / TRIG_MAX_RATIO);
	}
	
	// Init paths
	hand_path = gpath_create(&HAND_PATH_INFO);
	star_path = gpath_create(&STAR_PATH_INFO);
	
	// Push the window onto the stack
	window_stack_push(window, true);
	
	//Subscribe ticks
	tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);

	//Subscribe messages
	app_message_register_inbox_received(in_received_handler);
    app_message_register_inbox_dropped(in_dropped_handler);
    app_message_open(128, 128);
}
//-----------------------------------------------------------------------------------------------------------------------
static void deinit(void) 
{
	app_message_deregister_callbacks();
	tick_timer_service_unsubscribe();
	
	gpath_destroy(hand_path);
	gpath_destroy(star_path);
	
	window_destroy(window);
}
//-----------------------------------------------------------------------------------------------------------------------
int main(void) 
{
	init();
	app_event_loop();
	deinit();
}
//-----------------------------------------------------------------------------------------------------------------------