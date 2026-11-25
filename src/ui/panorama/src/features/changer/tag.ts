const interval = 0.1;

export function startTagLoop() {
  let pending: number | undefined;
  const step = () => {
    OpenChangerAPI.extras.tick();
    pending = $.Schedule(interval, step);
  };
  pending = $.Schedule(interval, step);
  return () => {
    if (pending !== undefined) $.CancelScheduled(pending);
    pending = undefined;
  };
}
