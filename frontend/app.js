/* global React, ReactDOM, ForgeCommon, Lanyard, ILSSAudio */
const { useState, useEffect, useRef, useCallback } = React;
const F = window.ForgeCommon;

/* ---------- channel definitions ---------- */
const COLORS = {
  red:    { rgb: '255, 49, 49',   label: 'Red' },
  green:  { rgb: '43, 226, 122',  label: 'Green' },
  blue:   { rgb: '56, 132, 255',  label: 'Blue' },
  teal:   { rgb: '22, 224, 208',  label: 'Teal' },
  purple: { rgb: '178, 84, 255',  label: 'Purple' },
  yellow: { rgb: '255, 210, 40',  label: 'Yellow' },
  orange: { rgb: '255, 138, 36',  label: 'Orange' },
};

const LED_PATTERNS = [
  { v: 'solid',  l: 'Solid' },
  { v: 'flash',  l: 'Single flash' },
  { v: 'alt',    l: 'Alternating' },
  { v: 'half',   l: 'Half-half' },
  { v: 'chase',  l: 'Chasing' },
  { v: 'off',    l: 'Off' },
];
const LED_LABELS = { solid:'Solid', flash:'Single flash', alt:'Alternating', half:'Half-half',
                     chase:'Chasing', off:'Off', pulse:'Pulsing', double:'Double flash' };

const HAPTIC_PATTERNS = [
  { v: 'solid', l: 'Solid' }, { v: 'pulse1', l: 'Pulse 1' }, { v: 'pulse2', l: 'Pulse 2' },
  { v: 'continuous', l: 'Continuous' }, { v: 'click', l: 'Click' }, { v: 'off', l: 'Off' },
];
const HAPTIC_LABELS = { solid:'Solid', pulse1:'Pulse 1', pulse2:'Pulse 2', continuous:'Continuous', click:'Click', off:'Off' };

const BUZZER_PATTERNS = [
  { v: 'alternating', l: 'Alternating' }, { v: 'silent', l: 'Silent' },
  { v: 'bs-sweep', l: 'BS sweep' }, { v: 'bs-fast-sweep', l: 'BS fast sweep' },
  { v: 'lf-buzz', l: 'LF buzz' }, { v: 'siren', l: 'Siren' },
  { v: 'code3-beep', l: 'Code-3 beep' }, { v: 'code3-sweep', l: 'Code-3 sweep' }, { v: 'code3-siren', l: 'Code-3 siren' },
];
const BUZZER_LABELS = { 'alternating':'Alternating', 'silent':'Silent', 'bs-sweep':'BS sweep',
  'bs-fast-sweep':'BS fast sweep', 'lf-buzz':'LF buzz', 'siren':'Siren', 'code3-beep':'Code-3 beep',
  'code3-sweep':'Code-3 sweep', 'code3-siren':'Code-3 siren', 'off':'Off' };
const BUZZER_DUR = { 'alternating':0.5, 'bs-sweep':1.0, 'bs-fast-sweep':0.3, 'lf-buzz':0.5, 'siren':0.28,
  'code3-beep':1.0, 'code3-sweep':1.0, 'code3-siren':1.0, 'silent':1.4, 'off':1.4 };

const IDLE = { color: 'green', led: 'solid', haptic: 'off', buzzer: 'silent', alert: 'none' };

/* ---------- inline icons ---------- */
const ic = (paths, vb = '0 0 24 24') => (p) =>
  React.createElement('svg', { viewBox: vb, fill: 'none', stroke: 'currentColor',
    strokeWidth: 2, strokeLinecap: 'round', strokeLinejoin: 'round', ...p },
    ...paths.map((d, i) => React.createElement('path', { key: i, d })));

const PersonIcon = ic(['M12 2a4 4 0 0 1 4 4v1a4 4 0 0 1-8 0V6a4 4 0 0 1 4-4Z', 'M4 21v-1a6 6 0 0 1 6-6h4a6 6 0 0 1 6 6v1']);
const FireIcon = ic(['M12 2c1 3 5 5 5 9a5 5 0 0 1-10 0c0-1.5.6-2.7 1.4-3.6C9 9 9.5 7.5 9 6c2 .5 2.5 2 3 4 .8-1.4 0-5 0-8Z']);
const ClearIcon = ic(['M5 12h14', 'M5 12a7 7 0 0 1 7-7 7 7 0 0 1 6 3.5', 'M19 12a7 7 0 0 1-7 7 7 7 0 0 1-6-3.5']);
const StopCircleIcon = ic(['M12 22c5.523 0 10-4.477 10-10S17.523 2 12 2 2 6.477 2 12s4.477 10 10 10Z', 'M9 9h6v6H9z']);
const AlertTriIcon = ic(['M12 3 2 20h20L12 3Z', 'M12 10v4', 'M12 17h.01']);
const SoundOnIcon = ic(['M11 5 6 9H2v6h4l5 4V5Z', 'M15.5 8.5a5 5 0 0 1 0 7', 'M18.5 5.5a9 9 0 0 1 0 13']);
const SoundOffIcon = ic(['M11 5 6 9H2v6h4l5 4V5Z', 'M22 9l-6 6', 'M16 9l6 6']);
const FlaskIcon = ic(['M9 3h6', 'M10 3v5.4L4.6 18a2 2 0 0 0 1.8 3h11.2a2 2 0 0 0 1.8-3L14 8.4V3', 'M7.7 14h8.6']);

/* ---------- feature flags (persisted) ---------- */
const FLAG_KEY = 'ilss-flags';
const FLAG_DEFAULTS = { mouseSwing: false, muteByDefault: true };
function loadFlags() {
  try { return { ...FLAG_DEFAULTS, ...JSON.parse(localStorage.getItem(FLAG_KEY) || '{}') }; }
  catch (_) { return { ...FLAG_DEFAULTS }; }
}
function saveFlags(f) { try { localStorage.setItem(FLAG_KEY, JSON.stringify(f)); } catch (_) {} }

/* ---------- app context (user profile + shared app data) ---------- */
const APP_PROFILE_KEY = 'ilss-profile';
const PROFILE_DEFAULTS = { name: 'A. Morgan', initials: 'AM' };
function loadProfile() {
  try { return { ...PROFILE_DEFAULTS, ...JSON.parse(localStorage.getItem(APP_PROFILE_KEY) || '{}') }; }
  catch (_) { return { ...PROFILE_DEFAULTS }; }
}
function saveProfile(p) { try { localStorage.setItem(APP_PROFILE_KEY, JSON.stringify(p)); } catch (_) {} }

const AppContext = React.createContext({ profile: PROFILE_DEFAULTS, setProfile: () => {} });

function AppProvider({ children }) {
  const [profile, setProfileState] = React.useState(loadProfile);
  const setProfile = React.useCallback((patch) => {
    setProfileState((p) => { const next = { ...p, ...patch }; saveProfile(next); return next; });
  }, []);
  return React.createElement(AppContext.Provider, { value: { profile, setProfile } }, children);
}

/* ---------- small building blocks ---------- */
function Panel({ eyebrow, children, style }) {
  const e = React.createElement;
  return e('div', { className: 'panel panel-pad', style },
    eyebrow && e('div', { className: 'panel-head' },
      e('span', { className: 'panel-eyebrow' }, eyebrow),
      e('span', { className: 'panel-rule' })),
    children);
}

function Chips({ items, value, onChange }) {
  const e = React.createElement;
  return e('div', { className: 'chips' },
    items.map((it) => e('button', {
      key: it.v, className: 'chip-btn' + (value === it.v ? ' sel' : ''),
      onClick: () => onChange(it.v),
    }, it.l)));
}

function AlertButton({ kind, armed, disabled, Icon, title, desc, onClick }) {
  const e = React.createElement;
  return e('button', { className: `alert-btn ${kind}` + (armed ? ' armed' : ''), disabled, onClick },
    e('span', { className: 'ab-icon' }, e(Icon, null)),
    e('span', { className: 'ab-text' },
      e('span', { className: 'ab-title' }, title),
      e('span', { className: 'ab-desc' }, desc)));
}

/* ---------- alert banner ---------- */
function AlertBanner({ level }) {
  const e = React.createElement;
  if (level === 'none') return null;
  const fire = level === 'fire';
  const rgb = fire ? '255,49,49' : '178,84,255';
  const style = {
    display: 'flex', alignItems: 'center', gap: 14, padding: '13px 24px',
    background: `linear-gradient(90deg, rgba(${rgb},0.32), rgba(${rgb},0.12) 60%, rgba(${rgb},0.04))`,
    borderBottom: `1px solid rgba(${rgb},0.6)`,
    color: '#fff', position: 'relative', zIndex: 25,
    animation: 'edgePulse 1.6s ease-in-out infinite', '--alert-rgb': rgb,
  };
  return e('div', { style },
    e('span', { style: { width: 30, height: 30, borderRadius: 8, display: 'grid', placeItems: 'center',
      background: `rgba(${rgb},0.25)`, color: `rgb(${rgb})`, flex: 'none' } },
      e(fire ? FireIcon : PersonIcon, { style: { width: 18, height: 18 } })),
    e('span', { style: { fontWeight: 700, fontSize: 14, letterSpacing: 0.3 } },
      fire ? 'FIRE EMERGENCY ACTIVE' : 'PERSONAL ALERT ACTIVE'),
    e('span', { style: { fontSize: 12.5, color: 'rgba(255,255,255,0.72)' } },
      fire ? '· Evacuation signalling engaged on wearable · responders notified'
           : '· Discreet assistance request raised from wearable'),
    e('span', { style: { marginLeft: 'auto', fontSize: 11, letterSpacing: 1.5, color: `rgb(${rgb})`, fontWeight: 700 } },
      fire ? 'CODE RED' : 'ASSIST'));
}

/* ---------- header ---------- */
function AppHeader({ muted }) {
  const e = React.createElement;
  const { Header, Logo, Avatar } = F;
  const { profile } = React.useContext(AppContext);
  const initials = (profile.initials || '').slice(0, 3).toUpperCase() || '?';
  return e('div', { className: 'app-header' },
    e(Header, {
      logo: e('div', { className: 'brand-line' },
        e(Logo, { icon: true, iconVariant: 'color', style: { transform: 'scale(0.95)' } }),
        e('div', { className: 'brand-divider' }),
        e('div', null,
          e('div', { className: 'brand-title' }, 'Inclusive Life Safety System'),
          e('div', { className: 'brand-sub' }, 'Smart Lanyard · Simulator'))),
      rightSlot: e('div', { className: 'header-right' },
        e(window.BleConnect, null),
        e(Avatar, { size: 'md', text: initials, alt: profile.name, interactive: true })),
    }));
}

/* ---------- experiments modal ---------- */
function FlagRow({ title, desc, badge, checked, onChange, id }) {
  const e = React.createElement;
  return e('div', { className: 'flag-row' },
    e('div', { className: 'flag-text' },
      e('div', { className: 'flag-title' }, title,
        badge && e('span', { className: 'flag-badge' }, badge)),
      e('div', { className: 'flag-desc' }, desc)),
    e('label', { className: 'flag-toggle', htmlFor: id },
      e('input', { type: 'checkbox', id, checked, onChange: (ev) => onChange(ev.target.checked) }),
      e('span', { className: 'flag-toggle-track' },
        e('span', { className: 'flag-toggle-thumb' }))));
}

function ExperimentsModal({ flags, setFlag, onClose }) {
  const e = React.createElement;
  const { Modal, ModalContent, Button, Control, Label, Input } = F;
  const { profile, setProfile } = React.useContext(AppContext);
  const [name, setName] = React.useState(profile.name);
  const [initials, setInitials] = React.useState(profile.initials);

  const handleDone = () => {
    const trimName = name.trim();
    const trimInit = initials.trim().slice(0, 3).toUpperCase();
    if (trimName || trimInit) setProfile({ name: trimName || profile.name, initials: trimInit || profile.initials });
    onClose();
  };
  return e(Modal, {
    backdrop: true, label: 'Feature flags', title: 'Experiments',
    onModalClose: onClose, onBackdropClick: onClose,
    footerRight: e(Button, { variant: 'primary', onClick: handleDone }, 'Done'),
  },
    e(ModalContent, { padded: true, style: { overflowY: 'auto', maxHeight: '72vh' } },
      e('div', { style: { minWidth: 440, maxWidth: 500, display: 'flex', flexDirection: 'column', gap: 0 } },
        e('p', { style: { margin: '0 0 12px', fontSize: 13, color: '#8b8d92' } },
          'Opt-in behaviours that are still being trialled. Changes are saved to this browser.'),
        e(FlagRow, {
          id: 'flag-swing', title: 'Mouse swing momentum', badge: 'Physics',
          desc: 'Sweep the cursor back and forth across the lanyard to build swing momentum. It tops out at a sensible maximum and gravity settles it back to rest.',
          checked: flags.mouseSwing, onChange: (v) => setFlag('mouseSwing', v),
        }),
        e('div', { className: 'flag-sep' }),
        e(FlagRow, {
          id: 'flag-mute', title: 'Mute audio by default',
          desc: 'Start with the buzzer output muted on load — the user must switch Audio output on themselves.',
          checked: flags.muteByDefault, onChange: (v) => setFlag('muteByDefault', v),
        }),
        e('div', { className: 'flag-sep', style: { margin: '4px 0' } }),
        e('div', { className: 'flag-section-head' }, 'User profile'),
        e('div', { style: { display: 'flex', gap: 12, marginTop: 8 } },
          e('div', { style: { flex: 1 } },
            e(Control, null,
              e(Label, { htmlFor: 'prof-name' }, 'Display name'),
              e(Input, { id: 'prof-name', value: name, placeholder: 'e.g. A. Morgan',
                onChange: (ev) => setName(ev.target.value) }))),
          e('div', { style: { width: 100 } },
            e(Control, null,
              e(Label, { htmlFor: 'prof-init' }, 'Initials'),
              e(Input, { id: 'prof-init', value: initials, placeholder: 'AM', maxLength: 3,
                onChange: (ev) => setInitials(ev.target.value.toUpperCase()) })))))));
}

/* ============================================================ */
function App() {
  const e = React.createElement;
  const [st, setSt] = useState(IDLE);
  const [flags, setFlags] = useState(loadFlags);
  const [muted, setMuted] = useState(() => loadFlags().muteByDefault);
  const [pressed, setPressed] = useState(null);
  const [expOpen, setExpOpen] = useState(false);

  const setFlag = useCallback((key, val) => {
    setFlags((f) => { const next = { ...f, [key]: val }; saveFlags(next); return next; });
    if (key === 'muteByDefault' && val) setMuted(true);  // enabling re-mutes immediately
  }, []);

  const setCh = (patch) => setSt((s) => ({ ...s, ...patch }));

  const firePreset = useCallback(() =>
    setSt({ color: 'red', led: 'double', haptic: 'continuous', buzzer: 'code3-sweep', alert: 'fire' }), []);
  const personalPreset = useCallback(() =>
    setSt({ color: 'purple', led: 'pulse', haptic: 'pulse2', buzzer: 'code3-siren', alert: 'personal' }), []);
  const clearFire = useCallback(() => setSt((s) => s.alert === 'fire' ? IDLE : s), []);
  const clearPersonal = useCallback(() => setSt((s) => s.alert === 'personal' ? IDLE : s), []);

  // device side-button presses
  const press = (which, fn) => { fn(); setPressed(which); setTimeout(() => setPressed(null), 220); };

  // drive audio
  useEffect(() => { ILSSAudio.set(st.buzzer); }, [st.buzzer]);
  useEffect(() => { ILSSAudio.setMuted(muted); }, [muted]);

  const ledRgb = COLORS[st.color].rgb;
  const ledOn = st.led !== 'off';
  const buzzerActive = st.buzzer !== 'silent' && st.buzzer !== 'off';
  const alertRgb = st.alert === 'fire' ? '255,49,49' : st.alert === 'personal' ? '178,84,255' : '255,49,49';

  return e('div', { className: 'app' },
    e('div', { className: 'space-bg' }),
    e('div', { className: 'grid-floor' }),
    e('div', { className: 'edge-glow' + (st.alert !== 'none' ? ' on' : ''), style: { '--alert-rgb': alertRgb } }),

    e(AppHeader, { muted }),
    e(AlertBanner, { level: st.alert }),

    e('div', { className: 'stage' },
      /* ---- LEFT: emergency controls + status ---- */
      e('div', { className: 'col-left' },
        e(Panel, { eyebrow: 'Emergency Controls' },
          e('div', { className: 'action-stack' },
            e(AlertButton, { kind: 'alert', armed: st.alert === 'personal', Icon: PersonIcon,
              title: 'Simulate Personal Alert', desc: 'Pulsing purple · haptics · buzzer',
              onClick: personalPreset }),
            e(AlertButton, { kind: 'clear', disabled: st.alert !== 'personal', Icon: st.alert === 'personal' ? StopCircleIcon : ClearIcon,
              title: 'Clear Personal Alert', desc: st.alert === 'personal' ? 'Tap to stand down the wearable' : 'No active personal alert',
              onClick: clearPersonal }),
            e(AlertButton, { kind: 'danger', armed: st.alert === 'fire', Icon: FireIcon,
              title: 'Simulate Fire Emergency', desc: 'Double-flash red · haptics · buzzer',
              onClick: firePreset }),
            e(AlertButton, { kind: 'clear', disabled: st.alert !== 'fire', Icon: st.alert === 'fire' ? StopCircleIcon : ClearIcon,
              title: 'Clear Fire Emergency', desc: st.alert === 'fire' ? 'Tap to stand down the wearable' : 'No active fire emergency',
              onClick: clearFire }))),

        e(Panel, { eyebrow: 'Device Telemetry' },
          e('div', { className: 'hud-grid' },
            e('div', { className: 'hud-cell' },
              e('div', { className: 'hud-label' }, 'State'),
              e('div', { className: 'hud-value', style: { color: st.alert === 'fire' ? '#ff6a60' : st.alert === 'personal' ? '#c98bff' : '#2be27a' } },
                st.alert === 'fire' ? 'Fire' : st.alert === 'personal' ? 'Alert' : 'Standby')),
            e('div', { className: 'hud-cell' },
              e('div', { className: 'hud-label' }, 'Edge LED'),
              e('div', { className: 'hud-value' },
                e('span', { className: 'hud-swatch', style: { color: `rgb(${ledRgb})`, background: ledOn ? `rgb(${ledRgb})` : '#333' } }),
                ledOn ? COLORS[st.color].label : 'Off')),
            e('div', { className: 'hud-cell' },
              e('div', { className: 'hud-label' }, 'Pattern'),
              e('div', { className: 'hud-value', style: { fontSize: 13 } }, LED_LABELS[st.led])),
            e('div', { className: 'hud-cell' },
              e('div', { className: 'hud-label' }, 'Haptic'),
              e('div', { className: 'hud-value', style: { fontSize: 13 } }, HAPTIC_LABELS[st.haptic])),
            e('div', { className: 'hud-cell', style: { gridColumn: '1 / -1' } },
              e('div', { className: 'hud-label' }, 'Buzzer'),
              e('div', { className: 'hud-value', style: { fontSize: 13, justifyContent: 'space-between' } },
                BUZZER_LABELS[st.buzzer],
                e('span', { style: { fontSize: 10.5, letterSpacing: 1, color: buzzerActive && !muted ? '#2be27a' : '#57628a' } },
                  muted ? 'MUTED' : buzzerActive ? 'SOUNDING' : 'QUIET')))))),

      /* ---- CENTRE: the lanyard ---- */
      e('div', { className: 'col-scene' },
        e(Lanyard, {
          ledRgb, ledPattern: st.led, ledOn, hapticPattern: st.haptic,
          buzzerActive, buzzerDur: BUZZER_DUR[st.buzzer] || 1.4, pressed,
          swingEnabled: flags.mouseSwing,
          onPressPersonal: () => press('personal', () => st.alert === 'personal' ? clearPersonal() : personalPreset()),
          onPressFire: () => press('fire', () => st.alert === 'fire' ? clearFire() : firePreset()),
        })),

      /* ---- RIGHT: advanced device controls ---- */
      e('div', { className: 'col-right' },
        e(Panel, { eyebrow: 'Advanced · Edge LED' },
          e('div', { className: 'ctrl-row' },
            e('div', { className: 'ctrl-label' }, 'Colour', e('span', { className: 'lab-tag' }, 'RGB DIFFUSER')),
            e('div', { className: 'swatches' },
              Object.keys(COLORS).map((k) => e('button', {
                key: k, className: 'swatch' + (st.color === k ? ' sel' : ''),
                style: { '--sw': COLORS[k].rgb }, 'aria-label': COLORS[k].label,
                onClick: () => setCh({ color: k, led: st.led === 'off' ? 'solid' : st.led, alert: 'none' }),
              })))),
          e('div', { className: 'ctrl-row' },
            e('div', { className: 'ctrl-label' }, 'Pattern'),
            e(Chips, { items: LED_PATTERNS, value: ['solid','flash','alt','half','chase','off'].includes(st.led) ? st.led : null,
              onChange: (v) => setCh({ led: v, alert: 'none' }) }))),

        e(Panel, { eyebrow: 'Advanced · Haptics' },
          e('div', { className: 'ctrl-label' }, 'Vibration pattern', e('span', { className: 'lab-tag' }, 'LRA MOTOR')),
          e(Chips, { items: HAPTIC_PATTERNS, value: HAPTIC_PATTERNS.some(p=>p.v===st.haptic) ? st.haptic : null,
            onChange: (v) => setCh({ haptic: v }) })),

        e(Panel, { eyebrow: 'Advanced · Buzzer' },
          e('div', { className: 'ctrl-label' }, 'Acoustic pattern', e('span', { className: 'lab-tag' }, 'PIEZO SOUNDER')),
          e(Chips, { items: BUZZER_PATTERNS, value: st.buzzer, onChange: (v) => setCh({ buzzer: v }) }),
          e('div', { className: 'sound-toggle', style: { marginTop: 14 } },
            e('span', { className: 'st-left' },
              e(muted ? SoundOffIcon : SoundOnIcon, null),
              'Audio output'),
            e(F.Toggle, { id: 'snd', checked: !muted, onChange: (ev) => setMuted(!ev.target.checked) }))))),

    /* ---- Experiments feature-flag launcher + modal ---- */
    e('div', { className: 'experiments-fab' },
      e(F.Button, { variant: 'secondary', onClick: () => setExpOpen(true) }, '⚗️ Experiments')),
    expOpen && e(ExperimentsModal, { flags, setFlag, onClose: () => setExpOpen(false) }));
}

ReactDOM.createRoot(document.getElementById('root')).render(
  React.createElement(AppProvider, null, React.createElement(App)));
