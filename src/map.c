#include <nbt.h>
#include <TeleClassic26/gameplay/map.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* NBT tree builder helpers (stdlib allocators for nbt_free compat) */
static nbt_node *nbt_make(nbt_type type, const char *name)
{
    nbt_node *n = calloc(1, sizeof(nbt_node));
    if (!n) { return NULL; }
    n->type = type;
    if (name) {
        n->name = strdup(name);
        if (!n->name) { free(n); return NULL; }
    }
    return n;
}

static nbt_node *nbt_make_compound(const char *name)
{
    nbt_node *n = nbt_make(TAG_COMPOUND, name);
    if (!n) { return NULL; }
    struct nbt_list *s = calloc(1, sizeof(struct nbt_list));
    if (!s) { nbt_free(n); return NULL; }
    s->data = NULL;
    INIT_LIST_HEAD(&s->entry);
    n->payload.tag_compound = s;
    return n;
}

/* Attaches child to compound. On ANY failure the child is freed by this
 * function so the caller never has to worry about orphaned nodes. */
static pboolean nbt_put(nbt_node *compound, nbt_node *child)
{
    if (!compound || !child) { nbt_free(child); return FALSE; }
    struct nbt_list *e = calloc(1, sizeof(struct nbt_list));
    if (!e) { nbt_free(child); return FALSE; }
    e->data = child;
    list_add_tail(&e->entry, &compound->payload.tag_compound->entry);
    return TRUE;
}

static nbt_node *nbt_make_byte(const char *name, int8_t v)
{ nbt_node *n = nbt_make(TAG_BYTE, name); if (n) { n->payload.tag_byte = v; } return n; }

static nbt_node *nbt_make_short(const char *name, int16_t v)
{ nbt_node *n = nbt_make(TAG_SHORT, name); if (n) { n->payload.tag_short = v; } return n; }

static nbt_node *nbt_make_int(const char *name, int32_t v)
{ nbt_node *n = nbt_make(TAG_INT, name); if (n) { n->payload.tag_int = v; } return n; }

static nbt_node *nbt_make_long(const char *name, int64_t v)
{ nbt_node *n = nbt_make(TAG_LONG, name); if (n) { n->payload.tag_long = v; } return n; }

static nbt_node *nbt_make_float(const char *name, float v)
{ nbt_node *n = nbt_make(TAG_FLOAT, name); if (n) { n->payload.tag_float = v; } return n; }

static nbt_node *nbt_make_string(const char *name, const char *v)
{
    nbt_node *n = nbt_make(TAG_STRING, name);
    if (!n) { return NULL; }
    n->payload.tag_string = strdup(v ? v : "");
    if (!n->payload.tag_string) { nbt_free(n); return NULL; }
    return n;
}

static nbt_node *nbt_make_byte_array(const char *name, const void *data, int32_t len)
{
    nbt_node *n = nbt_make(TAG_BYTE_ARRAY, name);
    if (!n) { return NULL; }
    n->payload.tag_byte_array.length = len;
    if (len > 0) {
        n->payload.tag_byte_array.data = malloc((size_t)len);
        if (!n->payload.tag_byte_array.data) { nbt_free(n); return NULL; }
        if (data) { memcpy(n->payload.tag_byte_array.data, data, (size_t)len); }
    }
    return n;
}

/* Color config load/save helpers */

static void load_color(nbt_node *parent, const char *tag, tc_map_color_config_t *c)
{
    nbt_node *compound = nbt_find_by_name(parent, tag);
    if (!compound) { return; }
    nbt_node *n;
    n = nbt_find_by_name(compound, "R"); if (n) { c->red   = n->payload.tag_short; }
    n = nbt_find_by_name(compound, "G"); if (n) { c->green = n->payload.tag_short; }
    n = nbt_find_by_name(compound, "B"); if (n) { c->blue  = n->payload.tag_short; }
}

/* Returns FALSE on alloc failure. On failure the partially-built color
 * compound is freed internally — it is NOT attached to parent. */
static pboolean save_color(nbt_node *parent, const char *tag, const tc_map_color_config_t *c)
{
    nbt_node *compound = nbt_make_compound(tag);
    if (!compound) { return FALSE; }
    if (!nbt_put(compound, nbt_make_short("R", c->red)) ||
        !nbt_put(compound, nbt_make_short("G", c->green)) ||
        !nbt_put(compound, nbt_make_short("B", c->blue))) {
        nbt_free(compound);
        return FALSE;
    }
    return nbt_put(parent, compound);
}

/* tc_map_finalize */

static void free_block_definition(ppointer data, ppointer user_data) {
    tc_map_block_definition_t *block_definition = (tc_map_block_definition_t *)data;
    p_free(block_definition->block_name);
    p_free(block_definition);
}

void tc_map_finalize(tc_map_t *map) {
    p_free(map->name);
    p_free(map->created_by_service);
    p_free(map->created_by_username);
    p_free(map->map_gen_software);
    p_free(map->map_gen_name);
    p_free(map->block_array);

    if (map->custom_blocks_extension) {
        p_free(map->custom_blocks_extension->fallback_blocks);
        p_free(map->custom_blocks_extension);
    }
    if (map->env_colors_extension) {
        p_free(map->env_colors_extension);
    }
    if (map->env_appearance_extension) {
        p_free(map->env_appearance_extension->texture_url);
        p_free(map->env_appearance_extension);
    }
    if (map->env_weather_extension) {
        p_free(map->env_weather_extension);
    }
    if (map->block_definition_extensions) {
        p_list_foreach(map->block_definition_extensions->block_definitions, free_block_definition, NULL);
        p_list_free(map->block_definition_extensions->block_definitions);
        p_free(map->block_definition_extensions);
    }
}

pboolean tc_map_load(tc_map_t *map, const pchar *path)
{
    nbt_node *root = nbt_parse_path(path);
    if (!root || root->type != TAG_COMPOUND) {
        nbt_free(root);
        return FALSE;
    }

    memset(map, 0, sizeof(tc_map_t));
    nbt_node *n;

    n = nbt_find_by_path(root, "ClassicWorld.FormatVersion");
    if (n) { map->format_version = n->payload.tag_byte; }

    n = nbt_find_by_path(root, "ClassicWorld.Name");
    if (n) {
        map->name = p_strdup(n->payload.tag_string);
        if (!map->name) { goto fail; }
    }

    n = nbt_find_by_path(root, "ClassicWorld.UUID");
    if (n && n->payload.tag_byte_array.length >= TC_THREADS_UUID_LEN) {
        memcpy(map->uuid, n->payload.tag_byte_array.data, TC_THREADS_UUID_LEN);
    }

    n = nbt_find_by_path(root, "ClassicWorld.X");
    if (n) { map->x_size = n->payload.tag_short; }
    n = nbt_find_by_path(root, "ClassicWorld.Y");
    if (n) { map->y_size = n->payload.tag_short; }
    n = nbt_find_by_path(root, "ClassicWorld.Z");
    if (n) { map->z_size = n->payload.tag_short; }

    n = nbt_find_by_path(root, "ClassicWorld.CreatedBy.Service");
    if (n) {
        map->created_by_service = p_strdup(n->payload.tag_string);
        if (!map->created_by_service) { goto fail; }
    }
    n = nbt_find_by_path(root, "ClassicWorld.CreatedBy.Username");
    if (n) {
        map->created_by_username = p_strdup(n->payload.tag_string);
        if (!map->created_by_username) { goto fail; }
    }

    n = nbt_find_by_path(root, "ClassicWorld.MapGenerator.Software");
    if (n) {
        map->map_gen_software = p_strdup(n->payload.tag_string);
        if (!map->map_gen_software) { goto fail; }
    }
    n = nbt_find_by_path(root, "ClassicWorld.MapGenerator.MapGeneratorName");
    if (n) {
        map->map_gen_name = p_strdup(n->payload.tag_string);
        if (!map->map_gen_name) { goto fail; }
    }

    n = nbt_find_by_path(root, "ClassicWorld.TimeCreated");
    if (n) { map->time_created = n->payload.tag_long; }
    n = nbt_find_by_path(root, "ClassicWorld.LastAccessed");
    if (n) { map->last_accessed = n->payload.tag_long; }
    n = nbt_find_by_path(root, "ClassicWorld.LastModified");
    if (n) { map->last_modified = n->payload.tag_long; }

    n = nbt_find_by_path(root, "ClassicWorld.Spawn.X");
    if (n) { map->spawn_x = n->payload.tag_short; }
    n = nbt_find_by_path(root, "ClassicWorld.Spawn.Y");
    if (n) { map->spawn_y = n->payload.tag_short; }
    n = nbt_find_by_path(root, "ClassicWorld.Spawn.Z");
    if (n) { map->spawn_z = n->payload.tag_short; }
    n = nbt_find_by_path(root, "ClassicWorld.Spawn.H");
    if (n) { map->spawn_heading = n->payload.tag_byte; }
    n = nbt_find_by_path(root, "ClassicWorld.Spawn.P");
    if (n) { map->spawn_pitch = n->payload.tag_byte; }

    n = nbt_find_by_path(root, "ClassicWorld.BlockArray");
    if (n) {
        int32_t len = n->payload.tag_byte_array.length;
        map->block_array = (pchar *)p_malloc((psize)len);
        if (!map->block_array) { goto fail; }
        memcpy(map->block_array, n->payload.tag_byte_array.data, (size_t)len);
    }

    /* CPE: CustomBlocks */
    nbt_node *ext = nbt_find_by_path(root, "ClassicWorld.Metadata.CPE.CustomBlocks");
    if (ext) {
        tc_map_custom_blocks_extension_t *cb = (tc_map_custom_blocks_extension_t *)
            p_malloc(sizeof(tc_map_custom_blocks_extension_t));
        if (!cb) { goto fail; }
        memset(cb, 0, sizeof(*cb));
        map->custom_blocks_extension = cb;

        n = nbt_find_by_name(ext, "ExtensionVersion");
        if (n) { cb->extension_version = n->payload.tag_int; }
        n = nbt_find_by_name(ext, "SupportLevel");
        if (n) { cb->support_level = n->payload.tag_short; }
        n = nbt_find_by_name(ext, "Fallback");
        if (n) {
            cb->fallback_blocks = (pchar *)p_malloc((psize)n->payload.tag_byte_array.length);
            if (!cb->fallback_blocks) { goto fail; }
            memcpy(cb->fallback_blocks, n->payload.tag_byte_array.data,
                   (size_t)n->payload.tag_byte_array.length);
        }
    }

    /* CPE: EnvColors */
    ext = nbt_find_by_path(root, "ClassicWorld.Metadata.CPE.EnvColors");
    if (ext) {
        tc_map_env_colors_extension_t *ec = (tc_map_env_colors_extension_t *)
            p_malloc(sizeof(tc_map_env_colors_extension_t));
        if (!ec) { goto fail; }
        memset(ec, 0, sizeof(*ec));
        map->env_colors_extension = ec;

        n = nbt_find_by_name(ext, "ExtensionVersion");
        if (n) { ec->extension_version = n->payload.tag_int; }
        load_color(ext, "Sky",      &ec->sky);
        load_color(ext, "Cloud",    &ec->cloud);
        load_color(ext, "Fog",      &ec->fog);
        load_color(ext, "Ambient",  &ec->ambient);
        load_color(ext, "Sunlight", &ec->sunlight);
    }

    /* CPE: EnvMapAppearance */
    ext = nbt_find_by_path(root, "ClassicWorld.Metadata.CPE.EnvMapAppearance");
    if (ext) {
        tc_map_env_appearance_extension_t *ea = (tc_map_env_appearance_extension_t *)
            p_malloc(sizeof(tc_map_env_appearance_extension_t));
        if (!ea) { goto fail; }
        memset(ea, 0, sizeof(*ea));
        map->env_appearance_extension = ea;

        n = nbt_find_by_name(ext, "ExtensionVersion");
        if (n) { ea->extension_version = n->payload.tag_int; }
        n = nbt_find_by_name(ext, "TextureURL");
        if (n) {
            ea->texture_url = p_strdup(n->payload.tag_string);
            if (!ea->texture_url) { goto fail; }
        }
        n = nbt_find_by_name(ext, "SideBlock");
        if (n) { ea->side_block = n->payload.tag_byte; }
        n = nbt_find_by_name(ext, "EdgeBlock");
        if (n) { ea->edge_block = n->payload.tag_byte; }
        n = nbt_find_by_name(ext, "SideLevel");
        if (n) { ea->side_level = n->payload.tag_short; }
    }

    /* CPE: EnvWeatherType */
    ext = nbt_find_by_path(root, "ClassicWorld.Metadata.CPE.EnvWeatherType");
    if (ext) {
        tc_map_env_weather_extension_t *ew = (tc_map_env_weather_extension_t *)
            p_malloc(sizeof(tc_map_env_weather_extension_t));
        if (!ew) { goto fail; }
        memset(ew, 0, sizeof(*ew));
        map->env_weather_extension = ew;

        n = nbt_find_by_name(ext, "ExtensionVersion");
        if (n) { ew->extension_version = n->payload.tag_int; }
        n = nbt_find_by_name(ext, "WeatherType");
        if (n) { ew->weather_type = n->payload.tag_byte; }
    }

    /* CPE: BlockDefinitions */
    ext = nbt_find_by_path(root, "ClassicWorld.Metadata.CPE.BlockDefinitions");
    if (ext && ext->type == TAG_COMPOUND) {
        tc_map_block_definition_extension_t *bd = (tc_map_block_definition_extension_t *)
            p_malloc(sizeof(tc_map_block_definition_extension_t));
        if (!bd) { goto fail; }
        memset(bd, 0, sizeof(*bd));
        map->block_definition_extensions = bd;

        n = nbt_find_by_name(ext, "ExtensionVersion");
        if (n) { bd->extension_version = n->payload.tag_int; }

        struct list_head *pos;
        list_for_each(pos, &ext->payload.tag_compound->entry) {
            struct nbt_list *le = list_entry(pos, struct nbt_list, entry);
            nbt_node *child = le->data;
            if (!child || child->type != TAG_COMPOUND ||
                !child->name || strncmp(child->name, "Block", 5) != 0) {
                continue;
            }

            tc_map_block_definition_t *def = (tc_map_block_definition_t *)
                p_malloc(sizeof(tc_map_block_definition_t));
            if (!def) { goto fail; }
            memset(def, 0, sizeof(*def));

            /* Prepend so we can detect p_list alloc failure (head always
             * changes on success). Reversed to restore order after the loop. */
            PList *new_head = p_list_prepend(bd->block_definitions, def);
            if (new_head == bd->block_definitions) {
                p_free(def);
                goto fail;
            }
            bd->block_definitions = new_head;

            n = nbt_find_by_name(child, "ID");
            if (n) { def->block_id = n->payload.tag_byte; }
            n = nbt_find_by_name(child, "Name");
            if (n) {
                def->block_name = p_strdup(n->payload.tag_string);
                if (!def->block_name) { goto fail; }
            }
            n = nbt_find_by_name(child, "Speed");
            if (n) { def->speed = n->payload.tag_float; }
            n = nbt_find_by_name(child, "Textures");
            if (n && n->payload.tag_byte_array.length >= 6) {
                memcpy(def->textures, n->payload.tag_byte_array.data, 6);
            }
            n = nbt_find_by_name(child, "TransmitsLight");
            if (n) { def->transmits_light = n->payload.tag_byte; }
            n = nbt_find_by_name(child, "WalkSound");
            if (n) { def->walk_sound = n->payload.tag_byte; }
            n = nbt_find_by_name(child, "Shape");
            if (n) { def->shape = n->payload.tag_byte; }
            n = nbt_find_by_name(child, "BlockDraw");
            if (n) { def->block_draw = n->payload.tag_byte; }
            n = nbt_find_by_name(child, "Fog");
            if (n && n->payload.tag_byte_array.length >= 4) {
                memcpy(def->fog, n->payload.tag_byte_array.data, 4);
            }
            n = nbt_find_by_name(child, "Coords");
            if (n && n->payload.tag_byte_array.length >= 6) {
                memcpy(def->coords, n->payload.tag_byte_array.data, 6);
            }
        }

        bd->block_definitions = p_list_reverse(bd->block_definitions);
    }

    nbt_free(root);
    return TRUE;

fail:
    tc_map_finalize(map);
    memset(map, 0, sizeof(tc_map_t));
    nbt_free(root);
    return FALSE;
}

pboolean tc_map_save(tc_map_t *map, const pchar *path)
{
    nbt_node *root = nbt_make_compound("ClassicWorld");
    if (!root) { return FALSE; }

    if (!nbt_put(root, nbt_make_byte("FormatVersion", map->format_version))) { goto fail; }
    if (!nbt_put(root, nbt_make_string("Name", map->name))) { goto fail; }
    if (!nbt_put(root, nbt_make_byte_array("UUID", map->uuid, TC_THREADS_UUID_LEN))) { goto fail; }
    if (!nbt_put(root, nbt_make_short("X", map->x_size))) { goto fail; }
    if (!nbt_put(root, nbt_make_short("Y", map->y_size))) { goto fail; }
    if (!nbt_put(root, nbt_make_short("Z", map->z_size))) { goto fail; }

    if (map->created_by_service || map->created_by_username) {
        nbt_node *cb = nbt_make_compound("CreatedBy");
        if (!cb) { goto fail; }
        if (!nbt_put(root, cb)) { goto fail; }
        if (map->created_by_service) {
            if (!nbt_put(cb, nbt_make_string("Service", map->created_by_service))) { goto fail; }
        }
        if (map->created_by_username) {
            if (!nbt_put(cb, nbt_make_string("Username", map->created_by_username))) { goto fail; }
        }
    }

    if (map->map_gen_software || map->map_gen_name) {
        nbt_node *mg = nbt_make_compound("MapGenerator");
        if (!mg) { goto fail; }
        if (!nbt_put(root, mg)) { goto fail; }
        if (map->map_gen_software) {
            if (!nbt_put(mg, nbt_make_string("Software", map->map_gen_software))) { goto fail; }
        }
        if (map->map_gen_name) {
            if (!nbt_put(mg, nbt_make_string("MapGeneratorName", map->map_gen_name))) { goto fail; }
        }
    }

    if (!nbt_put(root, nbt_make_long("TimeCreated", map->time_created))) { goto fail; }
    if (!nbt_put(root, nbt_make_long("LastAccessed", map->last_accessed))) { goto fail; }
    if (!nbt_put(root, nbt_make_long("LastModified", map->last_modified))) { goto fail; }

    nbt_node *spawn = nbt_make_compound("Spawn");
    if (!spawn) { goto fail; }
    if (!nbt_put(root, spawn)) { goto fail; }
    if (!nbt_put(spawn, nbt_make_short("X", map->spawn_x))) { goto fail; }
    if (!nbt_put(spawn, nbt_make_short("Y", map->spawn_y))) { goto fail; }
    if (!nbt_put(spawn, nbt_make_short("Z", map->spawn_z))) { goto fail; }
    if (!nbt_put(spawn, nbt_make_byte("H", map->spawn_heading))) { goto fail; }
    if (!nbt_put(spawn, nbt_make_byte("P", map->spawn_pitch))) { goto fail; }

    int32_t block_count = (int32_t)map->x_size * map->y_size * map->z_size;
    if (!nbt_put(root, nbt_make_byte_array("BlockArray", map->block_array, block_count))) { goto fail; }

    nbt_node *metadata = nbt_make_compound("Metadata");
    if (!metadata) { goto fail; }
    if (!nbt_put(root, metadata)) { goto fail; }

    if (map->custom_blocks_extension || map->env_colors_extension ||
        map->env_appearance_extension || map->env_weather_extension ||
        map->block_definition_extensions) {

        nbt_node *cpe = nbt_make_compound("CPE");
        if (!cpe) { goto fail; }
        if (!nbt_put(metadata, cpe)) { goto fail; }

        if (map->custom_blocks_extension) {
            nbt_node *cb = nbt_make_compound("CustomBlocks");
            if (!cb) { goto fail; }
            if (!nbt_put(cpe, cb)) { goto fail; }
            if (!nbt_put(cb, nbt_make_int("ExtensionVersion",
                         map->custom_blocks_extension->extension_version))) { goto fail; }
            if (!nbt_put(cb, nbt_make_short("SupportLevel",
                         map->custom_blocks_extension->support_level))) { goto fail; }
            if (map->custom_blocks_extension->fallback_blocks) {
                if (!nbt_put(cb, nbt_make_byte_array("Fallback",
                             map->custom_blocks_extension->fallback_blocks, 256))) { goto fail; }
            }
        }

        if (map->env_colors_extension) {
            nbt_node *ec = nbt_make_compound("EnvColors");
            if (!ec) { goto fail; }
            if (!nbt_put(cpe, ec)) { goto fail; }
            if (!nbt_put(ec, nbt_make_int("ExtensionVersion",
                         map->env_colors_extension->extension_version))) { goto fail; }
            if (!save_color(ec, "Sky",      &map->env_colors_extension->sky)) { goto fail; }
            if (!save_color(ec, "Cloud",    &map->env_colors_extension->cloud)) { goto fail; }
            if (!save_color(ec, "Fog",      &map->env_colors_extension->fog)) { goto fail; }
            if (!save_color(ec, "Ambient",  &map->env_colors_extension->ambient)) { goto fail; }
            if (!save_color(ec, "Sunlight", &map->env_colors_extension->sunlight)) { goto fail; }
        }

        if (map->env_appearance_extension) {
            nbt_node *ea = nbt_make_compound("EnvMapAppearance");
            if (!ea) { goto fail; }
            if (!nbt_put(cpe, ea)) { goto fail; }
            if (!nbt_put(ea, nbt_make_int("ExtensionVersion",
                         map->env_appearance_extension->extension_version))) { goto fail; }
            if (!nbt_put(ea, nbt_make_string("TextureURL",
                         map->env_appearance_extension->texture_url
                             ? map->env_appearance_extension->texture_url : ""))) { goto fail; }
            if (!nbt_put(ea, nbt_make_byte("SideBlock",
                         map->env_appearance_extension->side_block))) { goto fail; }
            if (!nbt_put(ea, nbt_make_byte("EdgeBlock",
                         map->env_appearance_extension->edge_block))) { goto fail; }
            if (!nbt_put(ea, nbt_make_short("SideLevel",
                         map->env_appearance_extension->side_level))) { goto fail; }
        }

        if (map->env_weather_extension) {
            nbt_node *ew = nbt_make_compound("EnvWeatherType");
            if (!ew) { goto fail; }
            if (!nbt_put(cpe, ew)) { goto fail; }
            if (!nbt_put(ew, nbt_make_int("ExtensionVersion",
                         map->env_weather_extension->extension_version))) { goto fail; }
            if (!nbt_put(ew, nbt_make_byte("WeatherType",
                         map->env_weather_extension->weather_type))) { goto fail; }
        }

        if (map->block_definition_extensions) {
            nbt_node *bd = nbt_make_compound("BlockDefinitions");
            if (!bd) { goto fail; }
            if (!nbt_put(cpe, bd)) { goto fail; }
            if (!nbt_put(bd, nbt_make_int("ExtensionVersion",
                         map->block_definition_extensions->extension_version))) { goto fail; }

            PList *cur = map->block_definition_extensions->block_definitions;
            for (; cur; cur = cur->next) {
                tc_map_block_definition_t *def = (tc_map_block_definition_t *)cur->data;
                char bname[32];
                snprintf(bname, sizeof(bname), "Block%u",
                         (unsigned)(unsigned char)def->block_id);

                nbt_node *blk = nbt_make_compound(bname);
                if (!blk) { goto fail; }
                if (!nbt_put(bd, blk)) { goto fail; }
                if (!nbt_put(blk, nbt_make_byte("ID", def->block_id))) { goto fail; }
                if (!nbt_put(blk, nbt_make_string("Name", def->block_name))) { goto fail; }
                if (!nbt_put(blk, nbt_make_float("Speed", def->speed))) { goto fail; }
                if (!nbt_put(blk, nbt_make_byte_array("Textures", def->textures, 6))) { goto fail; }
                if (!nbt_put(blk, nbt_make_byte("TransmitsLight", def->transmits_light))) { goto fail; }
                if (!nbt_put(blk, nbt_make_byte("WalkSound", def->walk_sound))) { goto fail; }
                if (!nbt_put(blk, nbt_make_byte("Shape", def->shape))) { goto fail; }
                if (!nbt_put(blk, nbt_make_byte("BlockDraw", def->block_draw))) { goto fail; }
                if (!nbt_put(blk, nbt_make_byte_array("Fog", def->fog, 4))) { goto fail; }
                if (!nbt_put(blk, nbt_make_byte_array("Coords", def->coords, 6))) { goto fail; }
            }
        }
    }

    FILE *fp = fopen(path, "wb");
    if (!fp) { goto fail; }
    nbt_status status = nbt_dump_file(root, fp, STRAT_GZIP);
    fclose(fp);
    nbt_free(root);
    return status == NBT_OK;

fail:
    nbt_free(root);
    return FALSE;
}
