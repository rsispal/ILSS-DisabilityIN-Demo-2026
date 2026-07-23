export type LogDirection = 'ly-web' | 'web-ly' | 'local';

export type LogLevel = 'E' | 'W' | 'I' | 'D';

export interface ParsedDeviceLog {
  level: LogLevel;
  direction: LogDirection;
  /** Short packed / event type label, e.g. TX CMD, ACK, FW, GATT */
  eventType: string;
  /** One-line human summary */
  summary: string;
  msgId?: number;
  codeHex?: string;
  /** ack | nak when this line is a response; empty otherwise */
  reply?: 'ack' | 'nak';
}

const CODE_LABELS: Record<number, string> = {
  0x01: 'TwinState',
  0x02: 'Heartbeat',
  0x10: 'Button',
  0x40: 'Pairing',
};

function codeLabel(codeHex?: string): string | undefined {
  if (!codeHex) return undefined;
  const n = Number.parseInt(codeHex, 16);
  if (Number.isNaN(n)) return codeHex;
  return CODE_LABELS[n] ?? `0x${codeHex}`;
}

function pickLevel(text: string): LogLevel {
  const c = (text[0] || 'I').toUpperCase();
  if (c === 'E' || c === 'W' || c === 'I' || c === 'D') return c;
  return 'I';
}

function extractMsgId(text: string): number | undefined {
  const m = text.match(/msgId[=:]?\s*(\d+)/i);
  return m ? Number(m[1]) : undefined;
}

function extractCode(text: string): string | undefined {
  const m = text.match(/code=0x([0-9a-f]+)/i);
  return m ? m[1].toLowerCase() : undefined;
}

/**
 * Best-effort parse of freeform BLE / firmware log lines into card fields.
 * Keeps working as new string formats appear without changing every call site.
 */
export function parseDeviceLog(text: string): ParsedDeviceLog {
  const level = pickLevel(text);
  const msgId = extractMsgId(text);
  const codeHex = extractCode(text);
  const codeName = codeLabel(codeHex);
  const trimmed = text.replace(/^[EWID]\s*/, '').trim();

  // Firmware Log characteristic (no client ble/sim prefix)
  if (!/\((ble|sim|DigitalTwinApp)\)/i.test(text)) {
    return {
      level,
      direction: 'ly-web',
      eventType: 'FW',
      summary: trimmed.slice(0, 120) || text.slice(0, 120),
      msgId,
      codeHex,
    };
  }

  if (/TX Command/i.test(text)) {
    const isAck = /flags=0x[0-9a-f]*8/i.test(text) || /\bACK\b/i.test(text);
    const flags = text.match(/flags=0x([0-9a-f]+)/i)?.[1];
    const flagNum = flags ? Number.parseInt(flags, 16) : 0;
    const reply =
      flagNum & 0x08 ? ('ack' as const) : flagNum & 0x40 ? ('nak' as const) : undefined;
    return {
      level,
      direction: 'web-ly',
      eventType: reply === 'ack' ? 'TX ACK' : reply === 'nak' ? 'TX NAK' : 'TX CMD',
      summary: [codeName, isAck ? 'reply' : 'command', msgId != null ? `#${msgId}` : null]
        .filter(Boolean)
        .join(' · '),
      msgId,
      codeHex,
      reply,
    };
  }

  if (/heartbeat TX/i.test(text)) {
    return {
      level,
      direction: 'web-ly',
      eventType: 'TX CMD',
      summary: `Heartbeat poll${msgId != null ? ` · #${msgId}` : ''}`,
      msgId,
      codeHex: codeHex ?? '02',
    };
  }

  if (/TX pair_req/i.test(text)) {
    return {
      level,
      direction: 'web-ly',
      eventType: 'PAIR',
      summary: 'Pair request',
    };
  }

  if (/TX unpair/i.test(text)) {
    return {
      level,
      direction: 'web-ly',
      eventType: 'PAIR',
      summary: 'Unpair request',
    };
  }

  if (/\bACK\b/i.test(text) && /msgId=/i.test(text)) {
    return {
      level,
      direction: 'ly-web',
      eventType: 'ACK',
      summary: [codeName ?? 'ok', msgId != null ? `msg #${msgId}` : null].filter(Boolean).join(' · '),
      msgId,
      codeHex,
      reply: 'ack',
    };
  }

  if (/\bNAK\b/i.test(text)) {
    const reason = text.match(/reason=([^\s]+)/i)?.[1];
    return {
      level,
      direction: 'ly-web',
      eventType: 'NAK',
      summary: [codeName, reason ? `reason ${reason}` : 'rejected', msgId != null ? `#${msgId}` : null]
        .filter(Boolean)
        .join(' · '),
      msgId,
      codeHex,
      reply: 'nak',
    };
  }

  if (/RX Event/i.test(text)) {
    return {
      level,
      direction: 'ly-web',
      eventType: 'RX EVT',
      summary: trimmed.replace(/^\(ble\)\s*/i, '').replace(/^RX Event\s*/i, 'Event '),
      msgId,
      codeHex,
    };
  }

  if (/RX Status/i.test(text)) {
    return {
      level,
      direction: 'ly-web',
      eventType: 'STATUS',
      summary: trimmed.replace(/^.*?RX Status\s*/i, 'Status '),
      msgId,
      codeHex,
    };
  }

  if (/RX Pairing/i.test(text)) {
    return {
      level,
      direction: 'ly-web',
      eventType: 'PAIR',
      summary: trimmed.replace(/^.*?RX Pairing\s*/i, 'Pairing '),
    };
  }

  if (/button press|Side button press/i.test(text)) {
    const side =
      text.match(/side=(\w+)/i)?.[1] ??
      text.match(/\((left|right)\)/i)?.[1] ??
      '?';
    return {
      level,
      direction: 'ly-web',
      eventType: 'BUTTON',
      summary: `Side ${side} press`,
      codeHex: codeHex ?? '10',
    };
  }

  if (/twin state/i.test(text)) {
    return {
      level,
      direction: 'ly-web',
      eventType: 'TWIN',
      summary: trimmed.replace(/^.*?twin state\s*/i, ''),
      msgId,
      codeHex: codeHex ?? '01',
    };
  }

  if (/sendTwinState/i.test(text)) {
    return {
      level,
      direction: 'web-ly',
      eventType: 'TWIN',
      summary: trimmed.replace(/^.*?sendTwinState\s*/i, 'Send '),
      msgId,
    };
  }

  if (/heartbeat alive/i.test(text)) {
    return {
      level,
      direction: 'ly-web',
      eventType: 'HB',
      summary: trimmed.replace(/^.*?heartbeat alive\s*/i, 'Alive '),
      msgId,
    };
  }

  if (/bad event frame/i.test(text)) {
    return {
      level,
      direction: 'ly-web',
      eventType: 'RX ERR',
      summary: 'Bad event frame',
    };
  }

  if (/link lost/i.test(text)) {
    return {
      level,
      direction: 'local',
      eventType: 'LINK',
      summary: trimmed.replace(/^.*?link lost\s*[—-]?\s*/i, 'Lost — '),
    };
  }

  if (
    /requestDevice|selected device|gatt\.connect|GATT connected|getPrimaryService|getCharacteristic|startNotifications|reading metadata|connected to|disconnect|pairing write|writePacket skipped|encodePacket|navigator\.bluetooth/i.test(
      text,
    )
  ) {
    return {
      level,
      direction: 'local',
      eventType: 'GATT',
      summary: trimmed.replace(/^\(ble\)\s*/i, ''),
    };
  }

  if (/\(sim\)/i.test(text)) {
    return {
      level,
      direction: 'local',
      eventType: 'SIM',
      summary: trimmed.replace(/^\(sim\)\s*/i, ''),
    };
  }

  if (/paired|unpaired/i.test(text)) {
    return {
      level,
      direction: 'ly-web',
      eventType: 'PAIR',
      summary: trimmed.replace(/^\(ble\)\s*/i, ''),
    };
  }

  return {
    level,
    direction: 'local',
    eventType: 'BLE',
    summary: trimmed.replace(/^\((ble|sim)\)\s*/i, ''),
    msgId,
    codeHex,
  };
}

export function directionLabel(d: LogDirection): string {
  switch (d) {
    case 'ly-web':
      return 'LY → WEB';
    case 'web-ly':
      return 'WEB → LY';
    default:
      return 'LOCAL';
  }
}
