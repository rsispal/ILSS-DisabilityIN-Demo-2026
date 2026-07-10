import { Button } from '@/ds/forge';

interface ExperimentsFabProps {
  onOpen: () => void;
  /** When device-logs feature is enabled, show a button to open the sidebar. */
  showLogs?: boolean;
  onOpenLogs?: () => void;
}

export function ExperimentsFab({ onOpen, showLogs, onOpenLogs }: ExperimentsFabProps) {
  return (
    <div className="experiments-fab">
      {showLogs && onOpenLogs ? (
        <Button variant="secondary" onClick={onOpenLogs}>
          Logs
        </Button>
      ) : null}
      <Button variant="secondary" onClick={onOpen}>
        ⚗️ Experiments
      </Button>
    </div>
  );
}
