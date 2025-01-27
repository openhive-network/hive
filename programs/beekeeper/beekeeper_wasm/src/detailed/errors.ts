/**
 * @internal
 */
export class BeekeeperError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "BeekeeperError";
  }
}
