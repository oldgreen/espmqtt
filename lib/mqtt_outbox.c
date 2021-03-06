#include "mqtt_outbox.h"
#include <stdlib.h>
#include <string.h>
#include "rom/queue.h"
#include "esp_log.h"

static const char *TAG = "OUTBOX";

outbox_handle_t outbox_init()
{
    outbox_handle_t outbox = calloc(1, sizeof(struct outbox_list_t));
    mem_assert(outbox);
    STAILQ_INIT(outbox);
    return outbox;
}

outbox_item_handle_t outbox_enqueue(outbox_handle_t outbox, uint8_t *data, int len, int msg_id, int msg_type, int tick)
{
    outbox_item_handle_t item = calloc(1, sizeof(outbox_item_t));
    mem_assert(item);
    item->msg_id = msg_id;
    item->msg_type = msg_type;
    item->tick = tick;
    item->len = len;
    item->buffer = malloc(len);
    mem_assert(item->buffer);
    memcpy(item->buffer, data, len);
    STAILQ_INSERT_TAIL(outbox, item, next);
    ESP_LOGD(TAG, "%s: INSERTED msgid=%d, msg_type=%d, len=%d, total len=%d", __func__, msg_id, msg_type, len, outbox_get_size(outbox));
    return item;
}

outbox_item_handle_t outbox_get(outbox_handle_t outbox, int msg_id)
{
    outbox_item_handle_t item;
    STAILQ_FOREACH(item, outbox, next) {
        if (item->msg_id == msg_id) {
            return item;
        }
    }
    return NULL;
}

outbox_item_handle_t outbox_dequeue(outbox_handle_t outbox)
{
    outbox_item_handle_t item;
    STAILQ_FOREACH(item, outbox, next) {
        if (!item->pending) {
            return item;
        }
    }
    return NULL;
}
esp_err_t outbox_delete(outbox_handle_t outbox, int msg_id, int msg_type)
{
    esp_err_t ret = ESP_FAIL;
    outbox_item_handle_t item, tmp;
    STAILQ_FOREACH_SAFE(item, outbox, next, tmp) {
        if (item->msg_id == msg_id && item->msg_type == msg_type) {
            STAILQ_REMOVE(outbox, item, outbox_item, next);
            ESP_LOGD(TAG, "%s: DELETED msgid=%d, msg_type=%d, len=%d, remaining total len=%d", __func__, msg_id, msg_type, item->len, outbox_get_size(outbox));
            free(item->buffer);
            free(item);
            ret = ESP_OK;
        }

    }
    return ret;
}
esp_err_t outbox_delete_msgid(outbox_handle_t outbox, int msg_id)
{
    outbox_item_handle_t item, tmp;
    STAILQ_FOREACH_SAFE(item, outbox, next, tmp) {
        if (item->msg_id == msg_id) {
            STAILQ_REMOVE(outbox, item, outbox_item, next);
            ESP_LOGD(TAG, "%s: DELETED msgid=%d, msg_type=%d, len=%d, remaining total len=%d", __func__, msg_id, item->msg_type, item->len, outbox_get_size(outbox));
            free(item->buffer);
            free(item);
        }

    }
    return ESP_OK;
}
esp_err_t outbox_set_pending(outbox_handle_t outbox, int msg_id)
{
    outbox_item_handle_t item = outbox_get(outbox, msg_id);
    if (item) {
        item->pending = true;
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t outbox_delete_msgtype(outbox_handle_t outbox, int msg_type)
{
    outbox_item_handle_t item, tmp;
    STAILQ_FOREACH_SAFE(item, outbox, next, tmp) {
        if (item->msg_type == msg_type) {
            STAILQ_REMOVE(outbox, item, outbox_item, next);
            ESP_LOGD(TAG, "%s: DELETED msgid=%d, msg_type=%d, len=%d, remaining total len=%d", __func__, item->msg_id, msg_type, item->len, outbox_get_size(outbox));
            free(item->buffer);
            free(item);
        }

    }
    return ESP_OK;
}

esp_err_t outbox_delete_expired(outbox_handle_t outbox, int current_tick, int timeout)
{
    outbox_item_handle_t item, tmp;
    STAILQ_FOREACH_SAFE(item, outbox, next, tmp) {
        if (current_tick - item->tick > timeout) {
            STAILQ_REMOVE(outbox, item, outbox_item, next);
            ESP_LOGD(TAG, "%s: DELETED msgid=%d, msg_type=%d, len=%d, remaining total len=%d", __func__, item->msg_id, item->msg_type, item->len, outbox_get_size(outbox));
            free(item->buffer);
            free(item);
        }

    }
    return ESP_OK;
}

int outbox_get_size(outbox_handle_t outbox)
{
    int siz = 0;
    outbox_item_handle_t item;
    STAILQ_FOREACH(item, outbox, next) {
        siz += item->len;
    }
    return siz;
}

esp_err_t outbox_cleanup(outbox_handle_t outbox, int max_size)
{
    while(outbox_get_size(outbox) > max_size) {
        outbox_item_handle_t item = outbox_dequeue(outbox);
        if (item == NULL) {
            return ESP_FAIL;
        }
        STAILQ_REMOVE(outbox, item, outbox_item, next);
        ESP_LOGD(TAG, "%s: DELETED msgid=%d, msg_type=%d, len=%d, remaining total len=%d", __func__, item->msg_id, item->msg_type, item->len, outbox_get_size(outbox));
        free(item->buffer);
        free(item);
    }
    return ESP_OK;
}

void outbox_destroy(outbox_handle_t outbox)
{
    outbox_cleanup(outbox, 0);
    free(outbox);
}
