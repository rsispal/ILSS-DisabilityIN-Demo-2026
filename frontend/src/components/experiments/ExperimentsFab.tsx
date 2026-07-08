import { Button } from '@/ds/forge';

interface ExperimentsFabProps {
  onOpen: () => void;
}

export function ExperimentsFab({ onOpen }: ExperimentsFabProps) {
  return (
    <div className="experiments-fab">
      <Button variant="secondary" onClick={onOpen}>
        ⚗️ Experiments
      </Button>
    </div>
  );
}
