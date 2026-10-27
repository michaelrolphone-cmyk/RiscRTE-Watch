/* Exact ordered damage replay. Unknown draws and bounded-work exhaustion use
 * the original full renderer; only a completed owned image is a reusable base. */
#define MAX_COMMANDS 1024u
#define TILE 16u
#ifndef WATCH_DAMAGE_UNSUPPORTED_RETRY
#define WATCH_DAMAGE_UNSUPPORTED_RETRY 4u
#endif
#if WATCH_DAMAGE_UNSUPPORTED_RETRY < 1 || WATCH_DAMAGE_UNSUPPORTED_RETRY > 64
#error "Unsupported-stream retry must remain bounded from 1 to 64 frames"
#endif
static unsigned damage_tiles_w , damage_tiles_h ;
#ifndef WATCH_DAMAGE_ALLOC
#define WATCH_DAMAGE_ALLOC malloc
#define WATCH_DAMAGE_FREE free
#endif
static bool damage_attempted ;
typedef struct {
    intptr_t key [ 8 ] ;
    char text [ 64 ] ;
    unsigned kind ;
    int bounds [ 4 ] ;
} damage_command ;
typedef struct {
    damage_command prior [ MAX_COMMANDS ] , current [ MAX_COMMANDS ] ;
    uint16_t * image ;
    unsigned char * dirty ;
    unsigned width , height , prior_count ;
    unsigned admission_id, unsupported_backoff;
    bool admission_id_valid;
    bool valid , stream_valid ;
} damage_store ;
static damage_store * damage_owner ;
static bool damage_unsupported ;
static unsigned damage_used ;
static void damage_bounds ( const damage_command * q , int * b ) {
    canvas c = {
        . width = damage_owner -> width , . height = damage_owner -> height
    } ;
    const intptr_t * k = q -> key ;
    b [ 0 ] = damage_owner -> width ;
    b [ 1 ] = damage_owner -> height ;
    b [ 2 ] = b [ 3 ] = 0 ;
    if ( q -> kind == 1 ) {
        int x1 = native_q ( & c , k [ 0 ] ) , y1 = native_q ( & c , k [ 1 ] ) , x2 = native_q ( & c , k [ 2 ] ) , y2 = native_q ( & c , k [ 3 ] ) , r = native_q ( & c , k [ 4 ] ) / 2 ;
        b [ 0 ] = ( ( x1 < x2 ? x1 : x2 ) - r - 16 ) / 16 ;
        b [ 1 ] = ( ( y1 < y2 ? y1 : y2 ) - r - 16 ) / 16 ;
        b [ 2 ] = ( ( x1 > x2 ? x1 : x2 ) + r + 16 ) / 16 + 1 ;
        b [ 3 ] = ( ( y1 > y2 ? y1 : y2 ) + r + 16 ) / 16 + 1 ;
    } else if ( q -> kind == 2 ) {
        int x = native_q ( & c , k [ 0 ] ) , y = native_q ( & c , k [ 1 ] ) , outer = native_q ( & c , k [ 2 ] ) + native_q ( & c , k [ 3 ] ) / 2 ;
        b [ 0 ] = ( x - outer - 16 ) / 16 ;
        b [ 1 ] = ( y - outer - 16 ) / 16 ;
        b [ 2 ] = ( x + outer + 16 ) / 16 + 1 ;
        b [ 3 ] = ( y + outer + 16 ) / 16 + 1 ;
    } else if ( q -> kind == 3 ) {
        b [ 0 ] = native_q ( & c , k [ 0 ] ) ;
        b [ 1 ] = native_q ( & c , k [ 1 ] ) ;
        b [ 2 ] = native_q ( & c , k [ 0 ] + k [ 2 ] ) ;
        b [ 3 ] = native_q ( & c , k [ 1 ] + k [ 3 ] ) ;
    } else if ( q -> kind == 4 ) {
        const face_font * f = ( const face_font * ) ( uintptr_t ) k [ 6 ] ;
        int width = 0 ;
        for ( const char * p = q -> text ; * p ; p ++ ) {
            const face_glyph * g = fglyph ( f , * p ) ;
            if ( g ) width += g -> advance + k [ 2 ] ;
        } if ( width ) {
            width -= k [ 2 ] ;
            int scale = k [ 3 ] ? k [ 3 ] * 65536 / width : 256 , pen = k [ 0 ] * 256 - width * scale / 512 ;
            for ( const char * p = q -> text ; * p ; p ++ ) {
                const face_glyph * g = fglyph ( f , * p ) ;
                if ( ! g ) continue ;
                int x = native_q ( & c , pen + g -> left * scale ) , y = native_q ( & c , ( k [ 1 ] + g -> top ) * 256 ) , sx = native_q ( & c , scale ) , sy = native_q ( & c , 256 ) ;
                int v [ 4 ] = {
                    floor_q8 ( x ) , floor_q8 ( y ) , floor_q8 ( x + g -> width * sx + 255 ) , floor_q8 ( y + g -> height * sy + 255 )
                } ;
                if ( v [ 0 ] < b [ 0 ] ) b [ 0 ] = v [ 0 ] ;
                if ( v [ 1 ] < b [ 1 ] ) b [ 1 ] = v [ 1 ] ;
                if ( v [ 2 ] > b [ 2 ] ) b [ 2 ] = v [ 2 ] ;
                if ( v [ 3 ] > b [ 3 ] ) b [ 3 ] = v [ 3 ] ;
                pen += ( g -> advance + k [ 2 ] ) * scale / 256 ;
            }
        }
    } for ( unsigned i = 0 ; i < 4 ; i ++ ) {
        if ( b [ i ] < 0 ) b [ i ] = 0 ;
        if ( b [ i ] > ( int ) ( i % 2 ? damage_owner -> height : damage_owner -> width ) ) b [ i ] = ( int ) ( i % 2 ? damage_owner -> height : damage_owner -> width ) ;
    }
} static void damage_execute ( canvas * c , const damage_command * q ) {
    const intptr_t * k = q -> key ;
    switch ( q -> kind ) {
        case 1 : fline_draw ( c , k [ 0 ] , k [ 1 ] , k [ 2 ] , k [ 3 ] , k [ 4 ] , k [ 5 ] , true ) ;
        break ;
        case 2 : fcirc_draw ( c , k [ 0 ] , k [ 1 ] , k [ 2 ] , k [ 3 ] , k [ 4 ] , true ) ;
        break ;
        case 3 : frect_draw ( c , k [ 0 ] , k [ 1 ] , k [ 2 ] , k [ 3 ] , k [ 4 ] , true ) ;
        break ;
        case 4 : ftext_alpha_draw ( c , ( const face_font * ) ( uintptr_t ) k [ 6 ] , q -> text , k [ 0 ] , k [ 1 ] , k [ 2 ] , k [ 3 ] , k [ 4 ] , k [ 5 ] , ( const uint8_t * ) ( uintptr_t ) k [ 7 ] , true ) ;
        break ;
        default : c -> failed = true ;
        break ;
    }
} static bool damage_barrier ( canvas * c ) {
    ( void ) c ;
    if ( ! damage_planning ) return false ;
    damage_unsupported = true ;
    return true ;
} static bool damage_record ( canvas * c , unsigned kind , const intptr_t * k , const char * s ) {
    if ( ! damage_planning ) return false ;
    if ( c -> failed || damage_unsupported ) return true ;
    if ( ! ( damage_used & 7u ) && ! canvas_checkpoint ( c ) ) return true ;
    if ( damage_used == MAX_COMMANDS || ( s && strlen ( s ) >= 64 ) ) {
        damage_unsupported = true ;
        return true ;
    } damage_command * q = damage_owner -> current + damage_used ++ ;
    memset ( q , 0 , sizeof ( * q ) ) ;
    q -> kind = kind ;
    memcpy ( q -> key , k , sizeof ( q -> key ) ) ;
    if ( s ) strcpy ( q -> text , s ) ;
    damage_bounds ( q , q -> bounds ) ;
    return true ;
} static bool damage_allowed ( unsigned x , unsigned y ) {
    return x < damage_owner -> width && y < damage_owner -> height && damage_owner -> dirty [ ( y / TILE ) * damage_tiles_w + x / TILE ] ;
} static void damage_mark ( const damage_command * q ) {
    const int * b = q -> bounds ;
    if ( b [ 2 ] <= b [ 0 ] || b [ 3 ] <= b [ 1 ] ) return ;
    for ( unsigned y = ( unsigned ) b [ 1 ] / TILE ; y <= ( unsigned ) ( b [ 3 ] - 1 ) / TILE ; y ++ ) for ( unsigned x = ( unsigned ) b [ 0 ] / TILE ; x <= ( unsigned ) ( b [ 2 ] - 1 ) / TILE ; x ++ ) damage_owner -> dirty [ y * damage_tiles_w + x ] = 1 ;
} static bool damage_intersects ( const damage_command * q ) {
    const int * b = q -> bounds ;
    if ( b [ 2 ] <= b [ 0 ] || b [ 3 ] <= b [ 1 ] ) return false ;
    for ( unsigned y = ( unsigned ) b [ 1 ] / TILE ; y <= ( unsigned ) ( b [ 3 ] - 1 ) / TILE ; y ++ ) for ( unsigned x = ( unsigned ) b [ 0 ] / TILE ; x <= ( unsigned ) ( b [ 2 ] - 1 ) / TILE ; x ++ ) if ( damage_owner -> dirty [ y * damage_tiles_w + x ] ) return true ;
    return false ;
} static bool damage_service_diff ( canvas * c , unsigned * steps ) {
    return ( ++ * steps & 31u ) || canvas_checkpoint ( c ) ;
} static bool damage_run ( risc_display_surface_v1 * s , const nova_watch_state * t , unsigned id ) {
    if ( ! damage_owner || s -> width != damage_owner -> width || s -> height != damage_owner -> height || s -> width != s -> height || s -> width > WATCH_LAYOUT_MAX_SIDE ) {
        if ( damage_owner ) { damage_owner -> valid = damage_owner -> stream_valid = false; damage_owner->admission_id_valid = false; damage_owner->unsupported_backoff = 0; }
        return face_render_full ( s , t , id ) ;
    }
    /* A failed collector is not repeated work worth paying on every frame.
     * This is observed stream capability, never a face-ID classification.
     * Selection changes reset admission; periodic retries discover conditional
     * streams that become supported as their state changes. Full rendering is
     * always authoritative and uses the original unmasked raster path. */
    if (!damage_owner->admission_id_valid || damage_owner->admission_id != id) {
        damage_owner->admission_id = id;
        damage_owner->admission_id_valid = true;
        damage_owner->unsupported_backoff = 0;
    }
    if (damage_owner->unsupported_backoff) {
        --damage_owner->unsupported_backoff;
        damage_owner->valid = damage_owner->stream_valid = false;
        return face_render_full(s, t, id);
    }
    damage_unsupported = false ;
    damage_used = 0 ;
    damage_planning = true ;
    bool planned = face_render_full ( s , t , id ) ;
    damage_planning = false ;
    if ( ! planned ) {
        damage_owner -> valid = damage_owner -> stream_valid = false ;
        return false ;
    } if ( damage_unsupported ) {
        damage_owner->unsupported_backoff = WATCH_DAMAGE_UNSUPPORTED_RETRY - 1u;
        damage_owner -> valid = damage_owner -> stream_valid = false ;
        return face_render_full ( s , t , id ) ;
    } canvas c = {
        ( uint8_t * ) damage_owner -> image , s -> width * 2 , s -> height , s -> width , t ? t -> capture_audio : NULL , t ? t -> capture_context : NULL , 0 , false , {
            0
        }
    } ;
    damage_tiles_w = ( s -> width + TILE - 1 ) / TILE ;
    damage_tiles_h = ( s -> height + TILE - 1 ) / TILE ;
    memset ( damage_owner -> dirty , 0 , damage_tiles_w * damage_tiles_h ) ;
    if ( ! damage_owner -> stream_valid ) {
        memset ( damage_owner -> dirty , 1 , damage_tiles_w * damage_tiles_h ) ;
    } else {
        unsigned steps = 0 , prefix = 0 ;
        while ( prefix < damage_used && prefix < damage_owner -> prior_count && ! memcmp ( damage_owner -> current + prefix , damage_owner -> prior + prefix , sizeof ( damage_command ) ) ) {
            if ( ! damage_service_diff ( & c , & steps ) ) {
                damage_owner -> valid = damage_owner -> stream_valid = false ;
                return false ;
            } prefix ++ ;
        } unsigned a = damage_owner -> prior_count , b = damage_used ;
        while ( a > prefix && b > prefix && ! memcmp ( damage_owner -> prior + a - 1 , damage_owner -> current + b - 1 , sizeof ( damage_command ) ) ) {
            if ( ! damage_service_diff ( & c , & steps ) ) {
                damage_owner -> valid = damage_owner -> stream_valid = false ;
                return false ;
            } a -- ;
            b -- ;
        } unsigned i = prefix , j = prefix ;
        /* Fixed lookahead and three-command exact anchors: O(N) with bounded constant.
 * Failure to reanchor conservatively marks the entire remaining middle. */
        while ( i < a && j < b ) {
            if ( ! damage_service_diff ( & c , & steps ) ) {
                damage_owner -> valid = damage_owner -> stream_valid = false ;
                return false ;
            } if ( ! memcmp ( damage_owner -> prior + i , damage_owner -> current + j , sizeof ( damage_command ) ) ) {
                i ++ ;
                j ++ ;
                continue ;
            } unsigned best_a = 0 , best_b = 0 , best = 99 ;
            for ( unsigned da = 0 ; da <= 8 && i + da + 2 < a ; da ++ ) for ( unsigned db = 0 ; db <= 8 && j + db + 2 < b ; db ++ ) {
                if ( ( ! da && ! db ) || da + db >= best ) continue ;
                if ( ! memcmp ( damage_owner -> prior + i + da , damage_owner -> current + j + db , 3 * sizeof ( damage_command ) ) ) {
                    best_a = da ;
                    best_b = db ;
                    best = da + db ;
                }
            } if ( best == 99 ) break ;
            for ( unsigned n = 0 ; n < best_a ; n ++ ) damage_mark ( damage_owner -> prior + i + n ) ;
            for ( unsigned n = 0 ; n < best_b ; n ++ ) damage_mark ( damage_owner -> current + j + n ) ;
            i += best_a ;
            j += best_b ;
        } for ( ; i < a ; i ++ ) {
            if ( ! damage_service_diff ( & c , & steps ) ) {
                damage_owner -> valid = damage_owner -> stream_valid = false ;
                return false ;
            } damage_mark ( damage_owner -> prior + i ) ;
        } for ( ; j < b ; j ++ ) {
            if ( ! damage_service_diff ( & c , & steps ) ) {
                damage_owner -> valid = damage_owner -> stream_valid = false ;
                return false ;
            } damage_mark ( damage_owner -> current + j ) ;
        }
    }
    /* Generic bounded work estimate: bounding-box sample area, no face IDs. */
    uint64_t total_work = 0 , replay_work = 0 ;
    unsigned estimate_steps = 0 ;
    for ( unsigned i = 0 ; i < damage_used ; i ++ ) {
        if ( ! damage_service_diff ( & c , & estimate_steps ) ) {
            damage_owner -> valid = damage_owner -> stream_valid = false ;
            return false ;
        } const int * b = damage_owner -> current [ i ] . bounds ;
        uint64_t area = b [ 2 ] > b [ 0 ] && b [ 3 ] > b [ 1 ] ? ( uint64_t ) ( b [ 2 ] - b [ 0 ] ) * ( b [ 3 ] - b [ 1 ] ) : 0 ;
        total_work += area ;
        if ( damage_intersects ( damage_owner -> current + i ) ) replay_work += area ;
    } if ( damage_owner -> stream_valid && total_work && replay_work * 5 > total_work * 4 ) {
        damage_owner -> valid = false ;
        bool ok = face_render_full ( s , t , id ) ;
        if ( ok ) {
            memcpy ( damage_owner -> prior , damage_owner -> current , damage_used * sizeof ( damage_command ) ) ;
            damage_owner -> prior_count = damage_used ;
            damage_owner -> stream_valid = true ;
        } else damage_owner -> stream_valid = false ;
        return ok ;
    } if ( ! damage_owner -> valid ) memset ( damage_owner -> dirty , 1 , damage_tiles_w * damage_tiles_h ) ;
    for ( unsigned y = 0 ; y < s -> height ; y ++ ) {
        if ( ! ( y & 7u ) && ! canvas_checkpoint ( & c ) ) {
            damage_owner -> valid = damage_owner -> stream_valid = false ;
            return false ;
        } for ( unsigned tx = 0 ; tx < damage_tiles_w ; tx ++ ) if ( damage_owner -> dirty [ ( y / TILE ) * damage_tiles_w + tx ] ) {
            unsigned left = tx * TILE , n = s -> width - left ;
            if ( n > TILE ) n = TILE ;
            memset ( c . pixels + y * c . stride + left * 2 , 0 , n * 2 ) ;
        }
    } for ( unsigned i = 0 ; i < damage_used && ! c . failed ; i ++ ) if ( damage_intersects ( damage_owner -> current + i ) ) damage_execute ( & c , damage_owner -> current + i ) ;
    if ( c . failed ) {
        damage_owner -> valid = damage_owner -> stream_valid = false ;
        return false ;
    } for ( unsigned y = 0 ; y < s -> height ; y ++ ) {
        if ( ! ( y & 7u ) && ! canvas_checkpoint ( & c ) ) {
            damage_owner -> valid = damage_owner -> stream_valid = false ;
            return false ;
        } memcpy ( ( uint8_t * ) s -> pixels + y * s -> stride_bytes , c . pixels + y * c . stride , s -> width * 2 ) ;
    } memcpy ( damage_owner -> prior , damage_owner -> current , damage_used * sizeof ( damage_command ) ) ;
    damage_owner -> prior_count = damage_used ;
    damage_owner -> valid = damage_owner -> stream_valid = true ;
    return true ;
} static void damage_release ( void ) {
    if ( damage_owner ) {
        if ( damage_owner -> image ) WATCH_DAMAGE_FREE ( damage_owner -> image ) ;
        WATCH_DAMAGE_FREE ( damage_owner ) ;
    } damage_owner = NULL ;
    damage_attempted = false ;
    damage_planning = false ;
}
/* Allocation callbacks belong to the existing invocation ledger. Capture is
 * checked before and after each optional allocation; failure never renders on
 * after lost authority. The outer Home lifecycle owns safe final release. */
static bool damage_prepare ( canvas * gate ) {
    if ( damage_attempted ) return true ;
    if ( ! canvas_checkpoint ( gate ) ) return false ;
    damage_attempted = true ;
    damage_owner = WATCH_DAMAGE_ALLOC ( sizeof ( * damage_owner ) ) ;
    if ( damage_owner ) memset ( damage_owner , 0 , sizeof ( * damage_owner ) ) ;
    if ( ! canvas_checkpoint ( gate ) ) return false ;
    if ( ! damage_owner ) return true ;
    damage_owner -> width = gate -> width ;
    damage_owner -> height = gate -> height ;
    size_t pixels = ( size_t ) gate -> width * gate -> height ;
    size_t tiles = ( ( gate -> width + TILE - 1 ) / TILE ) * ( ( gate -> height + TILE - 1 ) / TILE ) ;
    damage_owner -> image = WATCH_DAMAGE_ALLOC ( pixels * 2 + tiles ) ;
    if ( damage_owner -> image ) damage_owner -> dirty = ( unsigned char * ) ( damage_owner -> image + pixels ) ;
    if ( ! canvas_checkpoint ( gate ) ) return false ;
    return true ;
} bool nova_watch_face_render ( risc_display_surface_v1 * s , const nova_watch_state * state , unsigned id ) {
    if ( ! face_surface_valid ( s ) ) return face_render_full ( s , state , id ) ;
    nova_watch_state empty = {
        0
    } ;
    if ( ! state ) state = & empty ;
    canvas gate = {
        ( uint8_t * ) s -> pixels , s -> stride_bytes , s -> height , s -> width , state -> capture_audio , state -> capture_context , 0 , false , {
            0
        }
    } ;
    if ( ! damage_prepare ( & gate ) ) return false ;
    if ( ! damage_owner || ! damage_owner -> image ) return face_render_full ( s , state , id ) ;
    return damage_run ( s , state , id ) ;
}
#undef TILE
#undef MAX_COMMANDS
