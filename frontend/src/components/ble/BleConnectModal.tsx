import { useEffect, useRef, useState, type ReactNode } from 'react';
import { Modal, ModalContent, Button } from '@/ds/forge';
import { BtIcon, CloseIcon } from '@/lib/constants/icons';
import { useBleConnect } from '@/hooks/useBleConnect';
import { LanyardDeviceSummary } from '@/components/ble/LanyardDeviceSummary';

interface BleConnectModalProps {
  onClose: () => void;
}

type View = 'idle' | 'connecting' | 'error' | 'connected';

export function BleConnectModal({ onClose }: BleConnectModalProps) {
  const ble = useBleConnect();
  const [view, setView] = useState<View>(() =>
    ble.status === 'connected' ? 'connected' : ble.status === 'connecting' ? 'connecting' : 'idle',
  );
  const [errorText, setErrorText] = useState('');
  const [justConnected, setJustConnected] = useState(false);
  const prevStatus = useRef(ble.status);

  useEffect(() => {
    const prev = prevStatus.current;
    prevStatus.current = ble.status;

    if (ble.status === 'connecting') {
      setView('connecting');
      setJustConnected(false);
      return;
    }

    if (ble.status === 'connected') {
      setView('connected');
      if (prev === 'connecting') {
        setJustConnected(true);
        const t = window.setTimeout(() => setJustConnected(false), 1800);
        return () => window.clearTimeout(t);
      }
      return;
    }

    // disconnected
    if (prev === 'connecting') {
      setErrorText(
        ble.msg ||
          'Connection failed. Check the lanyard is powered and nearby, then try again.',
      );
      setView('error');
      return;
    }

    if (prev === 'connected') {
      setView('idle');
      setErrorText('');
      setJustConnected(false);
    }
  }, [ble.status, ble.msg]);

  const startPair = async () => {
    setErrorText('');
    setView('connecting');
    await ble.pair();
  };

  const startSimulate = () => {
    setErrorText('');
    setView('connecting');
    ble.simulate();
  };

  const doDisconnect = () => {
    ble.disconnect();
    setJustConnected(false);
    setView('idle');
    setErrorText('');
  };

  const busy = view === 'connecting';

  let title = 'Connect your lanyard';
  let prompt =
    'Pair over Bluetooth to mirror alerts, lights, haptics, and sound in real time.';
  let cta: ReactNode = null;

  if (view === 'connecting') {
    title = 'Connecting…';
    prompt =
      'Select your ILSS lanyard in the browser picker, then wait while we finish pairing.';
  } else if (view === 'error') {
    title = 'Couldn’t connect';
    prompt = errorText || 'Something went wrong while pairing.';
    cta = (
      <Button
        variant="primary"
        size="regular"
        onClick={() => void startPair()}
        style={{ width: '100%' }}
      >
        Try again
      </Button>
    );
  } else if (view === 'connected') {
    title = justConnected ? 'You’re connected' : 'Lanyard connected';
    prompt = justConnected
      ? 'Pairing succeeded. Twin state will stay in sync while this tab is open.'
      : 'This browser is linked to your wearable. Disconnect when you’re finished.';
    cta = (
      <Button
        variant="destructive-secondary"
        size="regular"
        onClick={doDisconnect}
        style={{ width: '100%' }}
      >
        Disconnect
      </Button>
    );
  } else {
    cta = (
      <>
        <Button
          variant="primary"
          size="regular"
          onClick={() => void startPair()}
          style={{ width: '100%' }}
        >
          Pair device
        </Button>
        {!ble.supported && (
          <Button
            variant="quiet-secondary"
            size="regular"
            onClick={startSimulate}
            style={{ width: '100%' }}
          >
            Simulate connection
          </Button>
        )}
      </>
    );
  }

  return (
    <Modal
      backdrop
      label="Bluetooth"
      title="Bluetooth"
      onModalClose={onClose}
      onBackdropClick={busy ? undefined : onClose}
      footerRight={
        <Button variant="quiet-secondary" onClick={onClose} disabled={busy}>
          Close
        </Button>
      }
    >
      <ModalContent padded>
        <div
          className={
            'ble-sheet' +
            (view === 'connected' ? ' live' : '') +
            (view === 'error' ? ' err' : '')
          }
        >
          <div className="ble-sheet-head">
            <div
              className={
                'ble-sheet-mark' +
                (view === 'connected' ? ' live' : '') +
                (view === 'connecting' ? ' busy' : '') +
                (view === 'error' ? ' err' : '')
              }
            >
              {view === 'connecting' ? (
                <span className="ble-sheet-spinner" aria-hidden />
              ) : view === 'connected' ? (
                <span className="ble-sheet-check" aria-hidden>
                  ✓
                </span>
              ) : view === 'error' ? (
                <CloseIcon style={{ width: 22, height: 22 }} />
              ) : (
                <BtIcon style={{ width: 24, height: 24 }} />
              )}
            </div>
            <div className="ble-sheet-copy">
              <h3 className="ble-sheet-title">{title}</h3>
              <p className="ble-sheet-prompt">{prompt}</p>
            </div>
          </div>

          {view === 'connecting' && (
            <div className="ble-sheet-progress" role="status" aria-live="polite">
              <div className="ble-sheet-progress-bar" />
              <span>Waiting for Bluetooth…</span>
            </div>
          )}

          {/* Device summary lives on the main left rail; only repeat it here once linked. */}
          {view === 'connected' && <LanyardDeviceSummary />}

          {cta && <div className="ble-sheet-actions">{cta}</div>}
        </div>
      </ModalContent>
    </Modal>
  );
}
