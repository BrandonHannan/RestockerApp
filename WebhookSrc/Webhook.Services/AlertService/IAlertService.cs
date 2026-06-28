using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Webhook.Data.Models;

namespace Webhook.Services.AlertService
{
    public interface IAlertService
    {
        public Task<Alert> GetAlertAsync(Guid alertId);
        public Task<List<Alert>> GetAlertsAsync();
        public Task AddOrUpdateAlert(Alert alert);
        public Task DeleteAlert(Guid alertId);
    }
}
