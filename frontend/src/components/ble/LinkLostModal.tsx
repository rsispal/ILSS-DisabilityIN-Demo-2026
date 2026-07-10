import { Modal, ModalContent, Button } from '@/ds/forge';
import { CloseIcon } from '@/lib/constants/icons';

interface LinkLostModalProps {
  onClose: () => void;
  onReconnect: () => void;
}

export function LinkLostModal({ onClose, onReconnect }: LinkLostModalProps) {
  return (
    <Modal
      backdrop
      label="Connection lost"
      title="Connection lost"
      onModalClose={onClose}
      onBackdropClick={onClose}
      footerRight={
        <>
          <Button variant="quiet-secondary" onClick={onClose}>
            Dismiss
          </Button>
          <Button variant="primary" onClick={onReconnect}>
            Pair again
          </Button>
        </>
      }
    >
      <ModalContent padded>
        <div className="ble-sheet err">
          <div className="ble-sheet-head">
            <div className="ble-sheet-mark err">
              <CloseIcon style={{ width: 22, height: 22 }} />
            </div>
            <div className="ble-sheet-copy">
              <h3 className="ble-sheet-title">Lanyard not responding</h3>
              <p className="ble-sheet-prompt">
                The twin link went quiet. Reset the physical lanyard (power cycle), wait for the
                blue ready flash, then pair again from this browser.
              </p>
            </div>
          </div>
        </div>
      </ModalContent>
    </Modal>
  );
}
