/*
    Sylverant Ship Server
    Copyright (C) 2013 Lawrence Sebald

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License version 3
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <sylverant/debug.h>

#include "subcmd.h"
#include "clients.h"
#include "ship_packets.h"
#include "utils.h"

static int handle_set_pos(ship_client_t *c, subcmd_set_pos_t *pkt) {
    lobby_t *l = c->cur_lobby;

    /* Save the new position and move along */
    if(c->client_id == pkt->client_id) {
        c->w = pkt->w;
        c->x = pkt->x;
        c->y = pkt->y;
        c->z = pkt->z;
    }

    return subcmd_send_lobby_dcnte(l, c, (subcmd_pkt_t *)pkt, 0);
}

static int handle_move(ship_client_t *c, subcmd_move_t *pkt) {
    lobby_t *l = c->cur_lobby;

    /* Save the new position and move along */
    if(c->client_id == pkt->client_id) {
        c->x = pkt->x;
        c->z = pkt->z;
    }

    return subcmd_send_lobby_dcnte(l, c, (subcmd_pkt_t *)pkt, 0);
}


int subcmd_dcnte_handle_bcast(ship_client_t *c, subcmd_pkt_t *pkt) {
    uint8_t type = pkt->type;
    lobby_t *l = c->cur_lobby;
    int rv, sent = 1, i;

    /* Ignore these if the client isn't in a lobby. */
    if(!l)
        return 0;

    pthread_mutex_lock(&l->mutex);

    switch(type) {
        case 0x36:
            rv = handle_set_pos(c, (subcmd_set_pos_t *)pkt);
            break;

        case 0x37:
        case 0x39:
            rv = handle_move(c, (subcmd_move_t *)pkt);
            break;

        default:
#ifdef LOG_UNKNOWN_SUBS
            debug(DBG_LOG, "Unknown 0x60: 0x%02X\n", type);
            print_packet((unsigned char *)pkt, LE16(pkt->hdr.dc.pkt_len));
#endif /* LOG_UNKNOWN_SUBS */
            sent = 0;
            break;

        case 0x1F:
            if(l->type == LOBBY_TYPE_DEFAULT) {
                for(i = 0; i < l->max_clients; ++i) {
                    if(l->clients[i] && l->clients[i] != c &&
                       subcmd_send_pos(c, l->clients[i])) {
                        rv = -1;
                        break;
                    }
                }
            }
            sent = 0;
            break;
    }

    /* Broadcast anything we don't care to check anything about. */
    if(!sent)
        rv = subcmd_send_lobby_dcnte(l, c, pkt, 0);

    pthread_mutex_unlock(&l->mutex);
    return rv;
}

int subcmd_translate_dc_to_nte(ship_client_t *c, subcmd_pkt_t *pkt) {
    uint8_t *sendbuf;
    uint8_t newtype = 0xFF;
    uint16_t len = LE16(pkt->hdr.dc.pkt_len);
    int rv;

    switch(pkt->type) {
        case SUBCMD_SET_AREA_21:
            newtype = 0x1D;
            break;

        case SUBCMD_FINISH_LOAD:
            newtype = 0x1F;
            break;

        case SUBCMD_SET_POS_3F:
            newtype = 0x36;
            break;

        case SUBCMD_MOVE_SLOW:
            newtype = 0x37;
            break;

        case SUBCMD_MOVE_FAST:
            newtype = 0x39;
            break;

        case SUBCMD_TALK_DESK:
            newtype = 0x46;
            break;

        default:
#ifdef LOG_UNKNOWN_SUBS
            debug(DBG_WARN, "Cannot translate DC->NTE packet, dropping\n");
            print_packet((unsigned char *)pkt, len);
#endif /* LOG_UNKNOWN_SUBS */
            return 0;
    }

    if(!(sendbuf = (uint8_t *)malloc(len)))
        return -1;

    memcpy(sendbuf, pkt, len);
    pkt = (subcmd_pkt_t *)sendbuf;
    pkt->type = newtype;

    rv = send_pkt_dc(c, (dc_pkt_hdr_t *)sendbuf);
    free(sendbuf);
    return rv;
}

int subcmd_translate_bb_to_nte(ship_client_t *c, bb_subcmd_pkt_t *pkt) {
    uint8_t *sendbuf;
    uint8_t newtype = 0xFF;
    uint16_t len = LE16(pkt->hdr.pkt_len);
    int rv;

    switch(pkt->type) {
        case SUBCMD_SET_AREA_21:
            newtype = 0x1D;
            break;

        case SUBCMD_FINISH_LOAD:
            newtype = 0x1F;
            break;

        case SUBCMD_SET_POS_3F:
            newtype = 0x36;
            break;

        case SUBCMD_MOVE_SLOW:
            newtype = 0x37;
            break;

        case SUBCMD_MOVE_FAST:
            newtype = 0x39;
            break;

        case SUBCMD_TALK_DESK:
            newtype = 0x46;
            break;

        default:
#ifdef LOG_UNKNOWN_SUBS
            debug(DBG_WARN, "Cannot translate BB->NTE packet, dropping\n");
            print_packet((unsigned char *)pkt, len);
#endif /* LOG_UNKNOWN_SUBS */
            return 0;
    }

    if(!(sendbuf = (uint8_t *)malloc(len)))
        return -1;

    memcpy(sendbuf, pkt, len);
    pkt = (bb_subcmd_pkt_t *)sendbuf;
    pkt->type = newtype;

    rv = send_pkt_bb(c, (bb_pkt_hdr_t *)sendbuf);
    free(sendbuf);
    return rv;
}

int subcmd_translate_nte_to_dc(ship_client_t *c, subcmd_pkt_t *pkt) {
    uint8_t *sendbuf;
    uint8_t newtype = 0xFF;
    uint16_t len = LE16(pkt->hdr.dc.pkt_len);
    int rv;

    switch(pkt->type) {
        case 0x1D:
            newtype = SUBCMD_SET_AREA_21;
            break;

        case 0x1F:
            newtype = SUBCMD_FINISH_LOAD;
            break;

        case 0x36:
            newtype = SUBCMD_SET_POS_3F;
            break;

        case 0x37:
            newtype = SUBCMD_MOVE_SLOW;
            break;

        case 0x39:
            newtype = SUBCMD_MOVE_FAST;
            break;

        case 0x46:
            newtype = SUBCMD_TALK_DESK;
            break;

        default:
#ifdef LOG_UNKNOWN_SUBS
            debug(DBG_WARN, "Cannot translate NTE->DC packet, dropping\n");
            print_packet((unsigned char *)pkt, len);
#endif /* LOG_UNKNOWN_SUBS */
            return 0;
    }

    if(!(sendbuf = (uint8_t *)malloc(len)))
        return -1;

    memcpy(sendbuf, pkt, len);
    pkt = (subcmd_pkt_t *)sendbuf;
    pkt->type = newtype;

    rv = send_pkt_dc(c, (dc_pkt_hdr_t *)sendbuf);
    free(sendbuf);
    return rv;
}

int subcmd_send_lobby_dcnte(lobby_t *l, ship_client_t *c, subcmd_pkt_t *pkt,
                            int igcheck) {
    int i;

    /* Send the packet to every connected client. */
    for(i = 0; i < l->max_clients; ++i) {
        if(l->clients[i] && l->clients[i] != c) {
            /* If we're supposed to check the ignore list, and this client is on
               it, don't send the packet. */
            if(igcheck && client_has_ignored(l->clients[i], c->guildcard)) {
                continue;
            }

            if(l->clients[i]->flags & CLIENT_FLAG_IS_DCNTE)
                send_pkt_dc(l->clients[i], (dc_pkt_hdr_t *)pkt);
            else
                subcmd_translate_nte_to_dc(l->clients[i], pkt);
        }
    }

    return 0;
}