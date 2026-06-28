using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data;
using Webhook.Data.Models;

namespace Webhook.Services.AlertService
{
    // NOTE: Alert does not inherit from BaseModel, so it has no IsDeleted/Updated fields.
    // Reads are not filtered on IsDeleted, updates do not stamp Updated, and Delete is a
    // hard delete rather than the soft delete used by the other entities.
    public class AlertService : BaseService, IAlertService
    {
        public AlertService(WebhookDbContext dbContext) : base(dbContext) { }

        public async Task<Alert> GetAlertAsync(Guid alertId)
        {
            return await _dbContext.Alerts.FirstOrDefaultAsync(a => a.AlertID == alertId);
        }

        public async Task<List<Alert>> GetAlertsAsync()
        {
            var alerts = await _dbContext.Alerts.ToListAsync();
            return alerts;
        }

        public async Task AddOrUpdateAlert(Alert alert)
        {
            IsValidAlert(alert);

            if (alert.AlertID == Guid.Empty)
            {
                alert.AlertID = Guid.NewGuid();
                alert.Created = DateTime.Now;

                _dbContext.Alerts.Add(alert);
            }
            else
            {
                var foundAlert = await _dbContext.Alerts.FirstOrDefaultAsync(a => a.AlertID == alert.AlertID);

                if (foundAlert == null)
                {
                    throw new Exception("Alert does not exist");
                }

                foundAlert.StockID = alert.StockID;
                foundAlert.StockChange = alert.StockChange;
            }

            await _dbContext.SaveChangesAsync();
        }

        public async Task DeleteAlert(Guid alertId)
        {
            var foundAlert = await _dbContext.Alerts.FirstOrDefaultAsync(a => a.AlertID == alertId);

            if (foundAlert == null)
            {
                throw new Exception("Alert does not exist");
            }

            _dbContext.Alerts.Remove(foundAlert);

            await _dbContext.SaveChangesAsync();
            return;
        }

        private bool IsValidAlert(Alert alert)
        {
            if (alert == null)
            {
                throw new ArgumentException("Invalid alert");
            }

            if (alert.StockID == Guid.Empty)
            {
                throw new ArgumentException("Invalid stock for new alert");
            }

            return true;
        }
    }
}
