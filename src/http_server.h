#ifndef HTTP_SERVER_H_INCLUDED
#define HTTP_SERVER_H_INCLUDED

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mongoose.h"
#define closesocket(x) close(x)

#include "scope_exit.h"
#include "curl.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_FAILURE_USERMSG
#define STBI_NO_HDR
#define STBI_MAX_DIMENSIONS (1<<11)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#include "stb_image.h"
#pragma GCC diagnostic pop
#endif // #ifndef

#ifndef STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "stb_image_resize2.h"
#pragma GCC diagnostic pop
#endif // #ifndef

#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif // #ifndef

#include "poppler/cpp/poppler-document.h"
#include "poppler/cpp/poppler-page.h"
#include "poppler/cpp/poppler-page-renderer.h"


static const char* HTTP_SERVER_BIND_ADDRESS = "0.0.0.0";
static const uint16_t HTTP_SERVER_BIND_PORT = 8000;
static const char* HTTP_SERVER_FQDN = "http://harvest-sigma.bnr.la";
static const char* HTTP_SERVER_DOC_ROOT = "./www-root/";

static const char* HTTP_SERVER_API_KEY = "1234567890ABCDEF";

static void start_thread(void *(*f)(void *), void *p) {
	pthread_t thread_id = (pthread_t) 0;
	pthread_attr_t attr;
	(void) pthread_attr_init(&attr);
	(void) pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread_id, &attr, f, p);
	pthread_attr_destroy(&attr);
}

struct thread_data {
	unsigned long conn_id;
	struct mg_mgr *mgr;

	struct mg_str content_type;
	struct mg_str api_key;
	struct mg_str uri;
	struct mg_str body;
};

struct http_response {
	int result;
	const char* str; // Must point to heap memory - freed in main thread.
};


#define STR_OR_NULL(ptr) ((ptr != NULL ? ptr : "(NULL)"))

static const char STAT_NAME_MAX_LENGTH = 32;

struct Stats {
	uint64_t member_id; // Called user_id when sent from sheet
	struct {
		char* name;
		int value;
		int next;
	} devotion;

	struct {
		char* name;
		int value;
		int next;
	} victory;

	struct {
		char* name;
		int value;
		int next;
	} trophies;

	struct {
		char* name;
		int value;
		int next;
	} hero;

	struct {
		char* name;
		int value;
		int next;
		bool is_shark;
	} shark;

	struct {
		double league; // TODO: Rename everywhere to chrono
		double bonus;
		double overall;
	} win_rate_recent;

	struct {
		double league;
		double bonus;
		double overall;
	} win_rate_all_time;
};

void print_stats(const Stats* s) {
	fprintf(stderr, "member_id : %lu\n", s->member_id);
	fprintf(stderr, "devotion.name  : %s\n", s->devotion.name);
	fprintf(stderr, "devotion.value : %d\n", s->devotion.value);
	fprintf(stderr, "devotion.next  : %d\n", s->devotion.next);

	fprintf(stderr, "victory.name  : %s\n", s->victory.name);
	fprintf(stderr, "victory.value : %d\n", s->victory.value);
	fprintf(stderr, "victory.next  : %d\n", s->victory.next);

	fprintf(stderr, "trophies.name  : %s\n", s->trophies.name);
	fprintf(stderr, "trophies.value : %d\n", s->trophies.value);
	fprintf(stderr, "trophies.next  : %d\n", s->trophies.next);

	fprintf(stderr, "hero.name  : %s\n", s->hero.name);
	fprintf(stderr, "hero.value : %d\n", s->hero.value);
	fprintf(stderr, "hero.next  : %d\n", s->hero.next);

	fprintf(stderr, "shark.name  : %s\n", s->shark.name);
	fprintf(stderr, "shark.value : %d\n", s->shark.value);
	fprintf(stderr, "shark.next  : %d\n", s->shark.next);
	fprintf(stderr, "shark.shark : %d\n", s->shark.is_shark);

	fprintf(stderr, "win_rate_recent.league : %f\n", s->win_rate_recent.league);
	fprintf(stderr, "win_rate_recent.bonus : %f\n", s->win_rate_recent.bonus);
	fprintf(stderr, "win_rate_recent.overall : %f\n", s->win_rate_recent.overall);

	fprintf(stderr, "win_rate_all_time.league : %f\n", s->win_rate_all_time.league);
	fprintf(stderr, "win_rate_all_time.bonus : %f\n", s->win_rate_all_time.bonus);
	fprintf(stderr, "win_rate_all_time.overall : %f\n", s->win_rate_all_time.overall);
}

http_response parse_stats(const mg_str json) {
	Stats stats;
	memset(&stats, 0, sizeof(Stats));

	{
		char* value = mg_json_get_str(json, "$.member_id");
		if(value != NULL) {
			stats.member_id = strtoull(value, NULL, 10);
			free(value);
		} else {
			return {400, mg_mprintf(R"({"result":"'member_id' key not found"})")};
		}
	}

	stats.devotion.name = mg_json_get_str(json, "$.devotion.name");
	if(stats.devotion.name == NULL) {
		return {400, mg_mprintf(R"({"result":"'devotion.name' key not found"})")};
	}
	SCOPE_EXIT(free(stats.devotion.name));

	stats.devotion.value = mg_json_get_long(json, "$.devotion.value", -1);
	if(stats.devotion.value == -1) {
		return {400, mg_mprintf(R"({"result":"'devotion.value' key not found"})")};
	}

	stats.devotion.next = mg_json_get_long(json, "$.devotion.next", -1);
	if(stats.devotion.next == -1) {
		return {400, mg_mprintf(R"({"result":"'devotion.next' key not found"})")};
	}

	stats.victory.name = mg_json_get_str(json, "$.victory.name");
	if(stats.victory.name == NULL) {
		return {400, mg_mprintf(R"({"result":"'victory.name' key not found"})")};
	}
	SCOPE_EXIT(free(stats.victory.name));

	stats.victory.value = mg_json_get_long(json, "$.victory.value", -1);
	if(stats.victory.value == -1) {
		return {400, mg_mprintf(R"({"result":"'victory.value' key not found"})")};
	}

	stats.victory.next = mg_json_get_long(json, "$.victory.next", -1);
	if(stats.victory.next == -1) {
		return {400, mg_mprintf(R"({"result":"'victory.next' key not found"})")};
	}

	stats.trophies.name = mg_json_get_str(json, "$.trophies.name");
	if(stats.trophies.name == NULL) {
		return {400, mg_mprintf(R"({"result":"'trophies.name' key not found"})")};
	}
	SCOPE_EXIT(free(stats.trophies.name));

	stats.trophies.value = mg_json_get_long(json, "$.trophies.value", -1);
	if(stats.trophies.value == -1) {
		return {400, mg_mprintf(R"({"result":"'trophies.value' key not found"})")};
	}

	stats.trophies.next = mg_json_get_long(json, "$.trophies.next", -1);
	if(stats.trophies.next == -1) {
		return {400, mg_mprintf(R"({"result":"'trophies.next' key not found"})")};
	}

	stats.hero.name = mg_json_get_str(json, "$.hero.name");
	if(stats.hero.name == NULL) {
		return {400, mg_mprintf(R"({"result":"'hero.name' key not found"})")};
	}
	SCOPE_EXIT(free(stats.hero.name));

	stats.hero.value = mg_json_get_long(json, "$.hero.value", -1);
	if(stats.hero.value == -1) {
		return {400, mg_mprintf(R"({"result":"'hero.value' key not found"})")};
	}

	stats.hero.next = mg_json_get_long(json, "$.hero.next", -1);
	if(stats.hero.next == -1) {
		return {400, mg_mprintf(R"({"result":"'hero.next' key not found"})")};
	}

	stats.shark.name = mg_json_get_str(json, "$.shark.name");
	if(stats.shark.name == NULL) {
		return {400, mg_mprintf(R"({"result":"'shark.name' key not found"})")};
	}
	SCOPE_EXIT(free(stats.shark.name));

	stats.shark.value = mg_json_get_long(json, "$.shark.value", -1);
	if(stats.shark.value == -1) {
		return {400, mg_mprintf(R"({"result":"'shark.value' key not found"})")};
	}

	stats.shark.next = mg_json_get_long(json, "$.shark.next", -1);
	if(stats.shark.next == -1) {
		return {400, mg_mprintf(R"({"result":"'shark.next' key not found"})")};
	}

	bool get_shark = mg_json_get_bool(json, "$.shark.is_shark", &stats.shark.is_shark);
	if(get_shark == false) {
		return {400, mg_mprintf(R"({"result":"'shark.is_shark' key not found"})")};
	}

	if(mg_json_get_num(json, "$.win_rate_recent.league", &stats.win_rate_recent.league) == false) {
		return {400, mg_mprintf(R"({"result":"'win_rate_recent.league' key not found"})")};
	}	

	if(mg_json_get_num(json, "$.win_rate_recent.bonus", &stats.win_rate_recent.bonus) == false) {
		return {400, mg_mprintf(R"({"result":"'win_rate_recent.bonus' key not found"})")};
	}	

	if(mg_json_get_num(json, "$.win_rate_recent.overall", &stats.win_rate_recent.overall) == false) {
		return {400, mg_mprintf(R"({"result":"'win_rate_recent.overall' key not found"})")};
	}	

	if(mg_json_get_num(json, "$.win_rate_all_time.league", &stats.win_rate_all_time.league) == false) {
		return {400, mg_mprintf(R"({"result":"'win_rate_all_time.league' key not found"})")};
	}	

	if(mg_json_get_num(json, "$.win_rate_all_time.bonus", &stats.win_rate_all_time.bonus) == false) {
		return {400, mg_mprintf(R"({"result":"'win_rate_all_time.bonus' key not found"})")};
	}	

	if(mg_json_get_num(json, "$.win_rate_all_time.overall", &stats.win_rate_all_time.overall) == false) {
		return {400, mg_mprintf(R"({"result":"'win_rate_all_time.overall' key not found"})")};
	}

	print_stats(&stats);

	return {200, mg_mprintf(R"({"result":"ok"})")};
}

static const size_t MAX_LEADERBOARD_ROWS = 100;
static const size_t MAX_WEEKS_PER_SEASON = 13;

struct Leaderboard {
   char* league;
   int season;
   struct Leaderboard_Row {
	   std::uint64_t member_id; // member_id will be 0 on first invalid row
	   int rank;
	   float average;
	   int drafts;
	   int trophies;
	   float win_rate;
	   int points[MAX_WEEKS_PER_SEASON];
   } rows[MAX_LEADERBOARD_ROWS];

   int row_count;
};

void print_leaderboard(const Leaderboard* l) {
	fprintf(stderr, "league: %s\n", l->league);
	fprintf(stderr, "season: %d\n", l->season);
	fprintf(stderr, "rows : [\n");
	for(size_t row = 0; row < MAX_LEADERBOARD_ROWS; ++row) {
		if(l->rows[row].member_id != 0) {
			fprintf(stderr, "{\n");
			fprintf(stderr, "	member_id: %lu\n", l->rows[row].member_id);
			fprintf(stderr, "	rank     : %d\n", l->rows[row].rank);
			fprintf(stderr, "	average  : %f\n", l->rows[row].average);
			fprintf(stderr, "	drafts   : %d\n", l->rows[row].drafts);
			fprintf(stderr, "	trophies : %d\n", l->rows[row].trophies);
			fprintf(stderr, "	win_rate : %f\n", l->rows[row].win_rate);
			fprintf(stderr, "	points   : [ ");
			for(size_t week = 0; week < MAX_WEEKS_PER_SEASON; ++week) {
				fprintf(stderr, "%d, ", l->rows[row].points[week]);
			}
			fprintf(stderr, "]\n");
			fprintf(stderr, "}\n");
		}
	}
	fprintf(stderr, "]\n");
}

http_response parse_leaderboards(const mg_str json) {
	Leaderboard leaderboard;
	memset(&leaderboard, 0, sizeof(Leaderboard));

	leaderboard.league = mg_json_get_str(json, "$.league");
	if(leaderboard.league == NULL) {
		return {400, mg_mprintf(R"({"result":"'league' key not found"})")};
	}
	SCOPE_EXIT(free(leaderboard.league));

	leaderboard.season = mg_json_get_long(json, "$.season", -1);
	if(leaderboard.season == -1) {
		return {400, mg_mprintf(R"({"result":"'season' key not found"})")};
	}

	while(true) {
		char key[12];
		snprintf(key, 12, "$.rows[%d]", leaderboard.row_count);

		int length;
		int offset = mg_json_get(json, key, &length);
		if(offset < 0) {
			if(offset == MG_JSON_NOT_FOUND) {
				break;
			} else {
				return {400, mg_mprintf(R"({"result":"JSON parse error"})")};
			}
		}

		const mg_str row = {json.ptr + offset, (size_t) length};

		{
			const char* value = mg_json_get_str(row, "$.member_id");
			if(value != NULL) {
				leaderboard.rows[leaderboard.row_count].member_id = strtoull(value, NULL, 10);
				free((void*)value);
			} else {
				return {400, mg_mprintf(R"({"result":"'%s.member_id' key not found"})", key)};
			}
		}

		leaderboard.rows[leaderboard.row_count].rank = mg_json_get_long(row, "$.rank", -1);
		if(leaderboard.rows[leaderboard.row_count].rank == -1) {
			return {400, mg_mprintf(R"({"result":"'%s.rank' key not found"})", key)};
		}

		leaderboard.rows[leaderboard.row_count].drafts = mg_json_get_long(row, "$.drafts", -1);
		if(leaderboard.rows[leaderboard.row_count].drafts == -1) {
			return {400, mg_mprintf(R"({"result":"'%s.drafts' key not found"})", key)};
		}

		leaderboard.rows[leaderboard.row_count].trophies = mg_json_get_long(row, "$.trophies", -1);
		if(leaderboard.rows[leaderboard.row_count].trophies == -1) {
			return {400, mg_mprintf(R"({"result":"'%s.trophies' key not found"})", key)};
		}

		{
			double value;
			if(mg_json_get_num(row, "$.average", &value) == false) {
				return {400, mg_mprintf(R"({"result":"'%s.average' key not found"})", key)};
			}
			leaderboard.rows[leaderboard.row_count].average = (float) value;
		}

		{
			double value;
			if(mg_json_get_num(row, "$.win_rate", &value) == false) {
				return {400, mg_mprintf(R"({"result":"'%s.win_rate' key not found"})", key)};
			}
			leaderboard.rows[leaderboard.row_count].win_rate = (float) value;
		}

		for(size_t week = 0; week < MAX_WEEKS_PER_SEASON; ++week) {
			char key[32];
			snprintf(key, 32, "$.points[%lu]", week);

			leaderboard.rows[leaderboard.row_count].points[week] = mg_json_get_long(row, key, -1);
		}

		leaderboard.row_count++;
	}

	print_leaderboard(&leaderboard);

	return {200, mg_mprintf(R"({"result":"ok"})")};
}

http_response make_thumbnail(const mg_str json) {
	static const int THUMBNAIL_SIZE = 50;

	const char* url = mg_json_get_str(json, "$.url");
	if(url == NULL) {
		return {400, strdup("{\"result\":\"malformed JSON\"}")};
	}
	SCOPE_EXIT(free((void*)url));

	const char* filename = NULL;
	for(size_t i = strlen(url)-1; i > 0; --i) {
		if(url[i] == '/') {
			filename = &url[i] + 1;
			break;
		}
	}

	char local_file_path[FILENAME_MAX];
	snprintf(local_file_path, FILENAME_MAX, "./static/badge_thumbnails/%s", filename);
	if(access(local_file_path, F_OK) == 0) {
		return {200, mg_mprintf(R"({"result":"%s"})", filename)};
	} else {
		auto buffer = download_file(url);
		if(has_value(buffer)) {
			SCOPE_EXIT(free(buffer.value.data));

			int width, height, channels;
			uint8_t* img = stbi_load_from_memory(buffer.value.data, buffer.value.size, &width, &height, &channels, 4);
			if(img != NULL) {
				SCOPE_EXIT(stbi_image_free(img));

				uint8_t* resized = (uint8_t*)alloca(THUMBNAIL_SIZE*THUMBNAIL_SIZE*4);
				stbir_resize_uint8_srgb(img, width, height, 0, resized, THUMBNAIL_SIZE, THUMBNAIL_SIZE, 0, STBIR_RGBA);

				snprintf(local_file_path, FILENAME_MAX, "static/badge_thumbnails/%s", filename);
				stbi_write_png_compression_level = 9;
				if(stbi_write_png(local_file_path, THUMBNAIL_SIZE, THUMBNAIL_SIZE, 4, resized, THUMBNAIL_SIZE*4) != 0) {
					return {201, mg_mprintf(R"({"result":"%s:%s/%s"})", HTTP_SERVER_FQDN, HTTP_SERVER_BIND_PORT, local_file_path)};
				} else {
					return {500, mg_mprintf(R"({"result":"%s"})", "saving file failed")};
				}
			} else {
				return {400, mg_mprintf(R"({"result":"%s"})", stbi_failure_reason())};
			}
		} else {
			return {400, mg_mprintf(R"({"result":"downloading url failed"})")};
		}
	}
}


http_response pdf_to_png(const mg_str json) {
	int width;
	{
		double value;
		if(mg_json_get_num(json, "$.width", &value) == true) {
			width = (int) value;
		} else {
			return {400, mg_mprintf(R"({"result":"'width' key not found"})")};
		}
	}

	int height;
	{
		double value;
		if(mg_json_get_num(json, "$.height", &value) == true) {
			height = (int) value;
		} else {
			return {400, mg_mprintf(R"({"result":"'height' key not found"})")};
		}
	}

	int dpi;
	{
		double value;
		if(mg_json_get_num(json, "$.dpi", &value) == true) {
			dpi = (int) value;
		} else {
			return {400, mg_mprintf(R"({"result":"'dpi' key not found"})")};
		}
	}

	uint64_t member_id;
	{
		char* value = mg_json_get_str(json, "$.member_id");
		if(value != NULL) {
			member_id = strtoull(value, NULL, 10);
			free(value);
		} else {
			return {400, mg_mprintf(R"({"result":"'member_id' key not found"})")};
		}
	}

	int mem_len = 0;
	char* mem = mg_json_get_b64(json, "$.bytes", &mem_len);
	if(mem == NULL) {
		return {400, mg_mprintf(R"({"result":"'member_id' key not found"})")};
	}
	SCOPE_EXIT(free(mem));

    poppler::document *pdf = poppler::document::load_from_raw_data(mem, mem_len);
    if(pdf == NULL) {
		return {400, mg_mprintf(R"({"result":"could not open PDF"})")};
	}
    SCOPE_EXIT(delete pdf);
    int page_count = pdf->pages();
    if(page_count == 0) {
		return {400, mg_mprintf(R"({"result":"no pages"})")};
	}
    poppler::page *page = pdf->create_page(0);
    SCOPE_EXIT(delete page);
    poppler::page_renderer renderer;
    poppler::image img = renderer.render_page(page, dpi, dpi, 0, 0, width, height);

	char file_path[FILENAME_MAX];
	snprintf(file_path, FILENAME_MAX, "static/badge_cards/%lu_%lu.png", member_id, time(NULL));
    img.save(file_path, "png");

	return {201, mg_mprintf(R"({"result":"%s:%d/%s"})", HTTP_SERVER_FQDN, HTTP_SERVER_BIND_PORT, file_path)};
}

http_response parse_commands(const mg_str json) {
	int index = 0;
	while(true) {
		char key[32];
		snprintf(key, 32, "$[%d]", index);

		int length;
		int offset = mg_json_get(json, key, &length);
		if(offset < 0) {
			if(offset == MG_JSON_NOT_FOUND) {
				break;
			} else {
				return {400, mg_mprintf(R"({"result":"JSON parse error"})")};
			}
		}
		const mg_str row = {json.ptr + offset, (size_t) length};

		const char* name = mg_json_get_str(row, "$.name");
		if(name == NULL) {
			return {400, mg_mprintf(R"({"result":"'name' key not found"})")};
		}
		SCOPE_EXIT(free((void*)name));

		const char* text = mg_json_get_str(row, "$.text");
		if(text == NULL) {
			return {400, mg_mprintf(R"({"result":"'text' key not found"})")};
		}
		SCOPE_EXIT(free((void*)text));

		bool team;
		if(mg_json_get_bool(row, "$.team", &team) == false) {
			return {400, mg_mprintf(R"({"result":"'team' key not found"})")};
		}

		// TODO: Add command to database
		fprintf(stderr, "command %d: %s, %d, %s\n", index, name, team, text);

		index++;
	}

	return {200, mg_mprintf(R"({"result":"ok"})")};
}

http_response parse_xmage_version(const mg_str json) {
	char* xmage_version = mg_json_get_str(json, "$.version");
	if(xmage_version == NULL) {
		return {400, mg_mprintf(R"({"result":"'version' key not found"})")};
	}
	SCOPE_EXIT(free(xmage_version));

	fprintf(stderr, "XMage version: %s\n", xmage_version);
	//database_update_xmage_version(...);

	return {200, mg_mprintf(R"({"result":"ok"})")};
}

// Handles POST requests
static void *post_thread_function(void *param) {
	thread_data *p = (thread_data*) param;

	// Free all resources that were passed in param
	SCOPE_EXIT(free((void*) p->content_type.ptr));
	SCOPE_EXIT(free((void*) p->api_key.ptr));
	SCOPE_EXIT(free((void*) p->uri.ptr));
	SCOPE_EXIT(free((void*) p->body.ptr));
	SCOPE_EXIT(free(p));

	MG_DEBUG(("Content-Type: %s\n", STR_OR_NULL(p->content_type.ptr)));
	MG_DEBUG(("API Key     : %s\n", STR_OR_NULL(p->api_key.ptr)));
	MG_DEBUG(("URI         : %s\n", STR_OR_NULL(p->uri.ptr)));
	MG_DEBUG(("Body        : %s\n", STR_OR_NULL(p->body.ptr)));

	struct http_response response;

	if(p->content_type.ptr == NULL || mg_strcmp(p->content_type, mg_str("application/json")) != 0) {
		response = {400, strdup("JSON payload required")};
	} else
	if(p->api_key.ptr == NULL || mg_strcmp(p->api_key, mg_str(HTTP_SERVER_API_KEY)) != 0) {
		response = {401, strdup("Invalid API key")};
	} else {
		if(mg_match(p->uri, mg_str("/api/v1/stats"), NULL)) {
			response = parse_stats(p->body);
		} else
		if(mg_match(p->uri, mg_str("/api/v1/leaderboards"), NULL)) {
			response = parse_leaderboards(p->body);
		} else
		if(mg_match(p->uri, mg_str("/api/v1/upload_commands"), NULL)) {
			response = parse_commands(p->body);
		} else
		if(mg_match(p->uri, mg_str("/api/v1/make_thumbnail"), NULL)) {
			response = make_thumbnail(p->body);
		} else
		if(mg_match(p->uri, mg_str("/api/v1/xmage_version"), NULL)) {
			response = parse_xmage_version(p->body);
		} else
		if(mg_match(p->uri, mg_str("/api/v1/pdf2png"), NULL)) {
			response = pdf_to_png(p->body);
		} else {
			response = {400, strdup("Invalid API endpoint")};
		}
	}

	mg_wakeup(p->mgr, p->conn_id, &response, sizeof(struct http_response));

	return NULL;
}

// HTTP request callback
static void http_server_func(struct mg_connection *con, int event, void *event_data, void *fn_data) {
	(void)fn_data;

	if (event == MG_EV_HTTP_MSG) {
		struct mg_http_message *message = (struct mg_http_message *) event_data;
		
		if(mg_match(message->method, mg_str("GET"), NULL)) {
			struct mg_http_serve_opts opts;
			memset(&opts, 0, sizeof(mg_http_serve_opts));
			opts.root_dir = HTTP_SERVER_DOC_ROOT;
			opts.extra_headers = "Cache-Control: public, max-age: 31536000\r\n";
			mg_http_serve_dir(con, message, &opts);
		} else
		if(mg_match(message->method, mg_str("POST"), NULL)) {
			thread_data *data = (thread_data*) calloc(1, sizeof(*data)); // Freed in worker thread
			if(data != NULL) {
				// Get the Content-Type and API_KEY from the HTTP headers.
				for(int i = 0; i < MG_MAX_HTTP_HEADERS && message->headers[i].name.len > 0; ++i) {
					MG_DEBUG(("header[%d]->%.*s:%.*s", i, message->headers[i].name.len, message->headers[i].name.ptr, message->headers[i].value.len, message->headers[i].value.ptr));
					if(mg_strcmp(message->headers[i].name, mg_str("Content-Type")) == 0) {
						data->content_type = mg_strdup(message->headers[i].value);
					} else
					if(mg_strcmp(message->headers[i].name, mg_str("API_KEY")) == 0) {
						data->api_key = mg_strdup(message->headers[i].value);
					}
				}

				data->conn_id = con->id;
				data->mgr     = con->mgr;
				data->uri     = mg_strdup(message->uri);
				data->body    = mg_strdup(message->body);

				start_thread(post_thread_function, data);  // Start thread and pass data
			} else {
				mg_http_reply(con, 500, NULL, "");
			}
		} else {
			mg_http_reply(con, 400, NULL, "");
		}
	} else
	if (event == MG_EV_WAKEUP) {
		struct http_response* response = (struct http_response*) ((struct mg_str*)event_data)->ptr;
		mg_http_reply(con,
				response->result,
				"",
				"%s\n", response->str);

		free((void*)response->str);
	}
}

mg_mgr g_mgr;

void http_server_init() {
	mg_log_level = MG_LL_DEBUG;
	mg_mgr_init(&g_mgr);
	mg_log_set(MG_LL_DEBUG);
	char listen[64];
	snprintf(listen, 64, "%s:%d", HTTP_SERVER_BIND_ADDRESS, HTTP_SERVER_BIND_PORT);
	mg_http_listen(&g_mgr, listen, http_server_func, NULL);
	mg_wakeup_init(&g_mgr);
}

void http_server_poll() {
	mg_mgr_poll(&g_mgr, 1000);
}

void http_server_free() {
	mg_mgr_free(&g_mgr);
}

#endif // HTTP_SERVER_H_INCLUDED
